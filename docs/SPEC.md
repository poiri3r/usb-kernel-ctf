# USB Kernel CTF Challenge Specification

> 이 문서는 출제자 및 개발 에이전트 전용이다.
> 참가자 배포본에 포함하지 않는다.

## 0. 개발 방향

2026-09-06에 다음 두 방향을 분리했다. **방향 2를 먼저 완성해 팀원에게
black-box 풀이를 요청하며, 방향 1은 폐기하지 않고 후속 hardware edition으로
보존한다.** 아래의 RP2040 관련 기존 설계와 검증 기록은 방향 1의 기준으로 계속
유효하다.

### 방향 1 — RP2040 hardware edition (보존)

- 참가자는 guest exploit과 RP2040 firmware를 함께 작성한다.
- 로컬에서 `KAPO{test_flag}`를 획득한 뒤 firmware/source, guest exploit/source,
  실행 절차와 hash를 ticket으로 제출한다.
- 운영진은 실제 flag가 든 disposable QEMU guest에서 참가자 exploit을 비특권으로
  먼저 실행하고, 참가자가 가져온 RP2040을 연결해 검증한다.
- 한 번 연결한 뒤 guest의 `KAPO_NEXT_STAGE` ioctl과 vendor request `0x5c`로
  helper leak, seq leak, Stage 2 모드를 자동 전환한다. GPIO나 수동 reset은 현재
  필수 설계가 아니다.
- 현재 상태는 firmware build와 host protocol self-test까지 PASS이며 physical
  RP2040 full chain은 NOT TESTED다.

### 방향 2 — QEMU-only Jeopardy edition (현재 우선순위)

- 참가자는 `KAPO{test_flag}`가 든 공개 QEMU target으로 로컬 exploit을 개발한다.
- 대회 서버는 TCP 연결마다 실제 flag가 든 동일 target의 disposable QEMU를 시작하고
  serial console을 참가자에게 전달한다. 수동 ticket 검증은 사용하지 않는다.
- 운영진이 제공하는 **trusted USB emulator** daemon이 guest 안에서 root로 자동
  시작되지만 처음에는 USB를 연결하지 않고 대기한다. UID 65534 participant exploit이
  helper-page grooming을 마친 뒤 readiness signal을 보내면 `dummy_hcd`와 Raw Gadget을
  통해 첫 모드를 연결한다.
- helper leak 뒤 participant exploit은 `KAPO_NEXT_STAGE`로 장치를 disconnect하고,
  seq grooming을 끝낸 뒤 readiness signal을 보내 seq mode를 reconnect한다. seq leak
  뒤에도 `KAPO_NEXT_STAGE`, disconnect, readiness signal 순으로 Stage 2 mode를 reconnect한다.
  단, Stage 2의 live pipe grooming과 hole 생성은 1024-byte configuration allocation이
  끝나고 driver가 bind된 **이후**, 취약한 ioctl을 호출하기 직전에 수행한다.
- 참가자는 USB emulator나 root process를 제출하지 않고, UID 65534에서 실행할
  guest exploit binary/source만 작성한다.
- participant release에는 `KAPO{test_flag}`만 둔다. 실제 flag가 든 image는 서버에만
  보관하고 참가자에게 파일로 전달하지 않는다.
- 참가자는 TCP 연결 직후 base64 ELF 한 줄을 launcher에 전송한다. launcher는 이를
  세션별 `/work/exploit`로만 저장하고, 이후 같은 연결을 UID 65534 serial shell로
  전환한다. 참가자 파일은 host에서 실행하지 않는다.
- participant-controlled emulator를 실제 flag가 있는 guest에서 root로 실행하지
  않는다. 그렇게 하면 exploit 없이 `/flag`를 읽을 수 있어 신뢰 경계가 깨진다.
- trusted emulator는 descriptor, disconnect/reconnect, participant가 ioctl 인자로 계산하고
  custom driver가 little-endian으로 직렬화·전송한 transfer context의 역직렬화와 그 값에
  따른 bulk payload 생성만 담당한다.
  helper/seq grooming, leak 판독, context 계산, pipe spray와 최종 trigger는 모두 UID
  65534 participant exploit이 담당한다.
- guest의 emulator executable은 root-owned mode `0700`이고 procfs는 `hidepid=2`로
  mount한다. 이는 UID 65534 guest process의 direct read와 root-process inspection을
  막는다. 로컬 participant archive 소유자는 initramfs 자체를 추출할 수 있으므로 emulator
  bytes를 암호학적으로 숨기는 경계는 아니며, 실제 서버 image 비공개 경계와 구분한다.
- 구현 상태: **Release candidate implementation, local runtime validation,
  independent review, and Git push complete; production deployment pending**.
  `testkernelctf/`와 `testkernelctf.tar.xz`는 방향 2 배포 후보이며, 재현성 재빌드
  artifact의 TCG cold boot full chain은 5/5다. heap 준비와 live pipe timing 힌트를
  제거한 이전 archive의 독립 AI black-box 풀이는 3/3 성공했다.
  full-entry overwrite 강화 archive의 fresh participant-only 풀이도 1,190초에 의도
  체인으로 test flag를 획득했다. 현재 QEMU-only Stage 2를 16-byte partial overwrite로
  축소한 후보는 organizer TCG cold boot 5/5를 통과했다. archive-only 독립
  AI 풀이 3개는 partial overwrite 구조와 최종 page-cache 목표를 복원했지만
  971초, 1,637초, 1,787초
  cutoff 내 guest flag를 얻지 못했다.
  `participant-release/`는 방향 1 RP2040 pre-release로 보존한다.
- Closed unintended shortcut: 세 독립 풀이 모두 helper stage에서 resize로 해제한 live
  pipe ring의 helper `struct page *`와 runtime `page_cache_pipe_buf_ops`를 함께 leak해
  seq stage의 KASLR leak을 사용하지 않았다. seq enumeration과 stage transition 자체는
  수행했다. target kernel은 resize가 복사한 old ring을 해제하기 전에 초기화하도록
  변경했고, 기존 세 shortcut exploit은 강화 artifact에서 3/3 leak 단계에 실패했다.
  일반 pipe close는 기존대로 entry별 `ops = NULL` 처리 후 ring을 해제하므로 의도한
  helper page-only leak은 유지한다. Stage 2 ioctl은 runtime
  `page_cache_pipe_buf_ops`와 정확히 일치하는 context만 허용해 다른 pipe ops를 쓰는
  등가 우회도 거부한다. partial 이전 full-entry 강화본의 fresh solve는 close 뒤
  stale ops가 0임을 확인하고 helper
  page-only leak, 별도 seq KASLR leak, exact page-cache ops를 모두 사용했다.

### 두 방향이 공유하는 고정 exploit chain

- Stage 1 취약점은 `usb/core/message.c`와 `usb/core/config.c`의 short
  configuration uninitialized heap disclosure다.
- Stage 2 취약점은 `usb/class/customdriver.c`의 `KAPO_FETCH_RECORDS` ioctl에서 allocation
  element 크기를 잘못 계산해 bulk data가 kmalloc object 경계를 넘는 heap overflow다.
  방향 1은 기존 128-record full-entry overwrite를 보존하고, 방향 2는 65 records로
  kmalloc-1k 경계를 16 bytes만 넘는다.
- `custom_id_table`이 `0xff/0x42/0x01` interface를 match하고,
  `custom_probe()`는 bulk IN endpoint를 확인한 뒤 `/dev/kapo_ctf0`을 등록한다.
  Stage 2 overflow 자체는 `probe()`가 아니라 ioctl이 trigger한다.
- 최종 primitive는 adjacent live `pipe_buffer` 재구성, SUID helper page-cache
  overwrite, UID 0과 `/flag` 획득으로 유지한다.

### 방향 2 완료 기준

- trusted USB emulator daemon이 boot 때 자동으로 시작해 disconnected 상태로
  대기하고, participant exploit의 readiness signal 뒤 세 모드를 안정적으로 제공한다.
- emulator binary에는 helper/seq/pipe grooming이나 flag 접근 기능이 없고 모든
  exploit primitive 준비는 UID 65534 participant exploit이 수행한다.
- participant exploit은 `/work`에서 UID 65534로 실행되며 organizer harness나
  solve 없이 `KAPO{test_flag}`를 획득할 수 있다.
- 사용자 지정 안정성 범위인 cold boot 5/5에서 full chain이 성공한다.
- 배포 archive에 이 문서, `STATUS.md`, `HANDOFF.md`, organizer exploit/harness,
  실제 flag가 포함되지 않는다.
- final release에서 `System.map`을 제외하고 `vmlinux` ELF symbol/debug table과 helper
  symbol을 strip한다. `CONFIG_KALLSYMS_ALL`은 비활성화하며 참가자에게 필요한 최소
  USB emulator contract만 README에 공개한다.
- 내부 검증 후 팀원이 participant archive만 받아 hardware 없이 독립적으로
  풀이하고 난이도와 누락 파일을 보고한다.
- server launcher는 TCP 연결마다 base64 ELF 하나를 받은 뒤 새 QEMU를 시작하고,
  연결 종료나 제한시간 만료 시 해당 QEMU를 종료한다. QEMU monitor와 server
  filesystem은 노출하지 않으며, exploit 하나만 든 세션별 임시 directory를 `/work`로
  전달한 뒤 폐기한다.

현재 판정은 **RC 기능 구현과 로컬 검증은 PASS, release gate와 대회 운영
배포는 PENDING**이다.
최종 partial archive를 외부 참가자가 완주한 성공 표본은 없으며, 3개 AI 세션은
partial 구조를 복원했지만 30분 내 flag를 얻지 못했다. 따라서 위 독립
풀이 완료 기준은 현재 미충족이다.

## 1. 문제 개요

- 문제 이름: KAPO USB Kernel CTF
- 분야: Pwn / Kernel
- 목표 난이도: 상
- 실행 환경: Ubuntu 24.04 + Linux Kernel 6.8
- 공격 입력 장치: 방향 1은 RP2040 Zero, 방향 2는 organizer-owned trusted Raw Gadget
  emulator
- 플래그 형식: `KAPO{...}`

### 한 줄 설명

방향 1에서는 RP2040, 방향 2에서는 trusted Raw Gadget emulator가 제공하는 USB 입력으로
취약한 Linux USB 커널 드라이버를 공격하고 최종적으로 플래그를 읽는다.

이 문제는 Linux USB core의 short configuration 처리 취약점과 CTF용 custom USB
driver의 취약점을 순차적으로 연결한다. `usb/core/message.c`가 광고 길이의 configuration
buffer를 초기화하지 않고, `usb/core/config.c`가 실제 수신 길이보다 큰 광고 길이를 raw
descriptor reader에 보존하는 조합으로 첫 번째 primitive가 발생한다. parser 자체에는
실제 수신 길이만 전달된다. 이후 USB enumeration이 계속되고 custom driver의 `probe()`가
device node를 등록한 뒤 participant exploit의 ioctl이 두 번째 primitive를 trigger한다.

## 2. 출제 의도
이 문제를 통해 참가자가 경험해야 하는 핵심 내용은 다음과 같다

- Linux USB 드라이버가 장치를 인식하는 일련의 과정
- `probe()`와 `device_add()` 및 USB 요청 처리 과정
- 커널 드라이버에서 발생하는 메모리 취약점
- 제한된 primitive를 exploit chain으로 발전시키는 과정
- 방향 1에서는 RP2040 펌웨어, 방향 2에서는 제공된 USB emulator protocol을 이용한
  공격 입력 제어

## 3. 문제 운영 및 검증 방식

참가자는 KAIST와 포항공대 학생 각 11명으로 구성된 총 2팀이며, 팀별로 서로 다른 방에 배정된다.

참가자는 로컬 환경에서 exploit을 개발해 `KAPO{test_flag}`를 획득한다. 대회에서는
challenge TCP endpoint에 접속하면 실제 flag가 든 disposable QEMU의 UID 65534 serial
shell을 받는다. 참가자는 미리 업로드된 `/work/exploit`을 실행하고 획득한 flag를
Jeopardy scoreboard에 제출한다.

## 4. 시스템 구성
### Host

- OS: Ubuntu 24.04
- Kernel: Linux 6.8.x
- Architecture: x86_64
- 실행 방식: VM
- initramfs 사용 여부: 사용. 공개 배포본은 `KAPO{test_flag}`, 운영 서버는 private
  initramfs에 실제 flag를 주입한다.

### USB input device

- 방향 1: RP2040 Zero
- 방향 2: organizer-owned trusted Raw Gadget emulator와 `dummy_hcd`

### Kernel code

- 원본 USB 코드: `usb/`
- custom driver: `usb/class/customdriver.c`
- QEMU-only shortcut hardening overlay: `kernel-patches/0001-pipe-clear-resized-ring.patch`
  (`fs/pipe.c`의 resized old ring만 해제 전 초기화)

## 5. 현재 확정된 공통 단계

- 공격 입력은 방향 1의 RP2040 Zero 또는 방향 2의 organizer-owned trusted emulator가
  USB transport로 전달한다.
- Linux USB core의 configuration descriptor 처리 경로에 Stage 1 취약점을 구현한다.
- 첫 번째 취약점은 custom driver의 `probe()` 이전에 발생한다.
- 특정 장치/interface/descriptor 조건을 만족하면 custom driver가 매칭된다.
- `probe()`는 device node를 등록하고 participant exploit의 ioctl이 추가 primitive를
  trigger한다.
- 최종 목표는 커널 권한 획득 및 flag 읽기다.

## 6. Exploit Chain

> Stage 1 leak, Stage 2 overflow, 인접 live object 도달과 page-cache overwrite를
> 연결한 QEMU full chain은 Confirmed다. source-level 단서 완화 후에도 object의 크기
> 불일치와 QEMU runtime regression을 재검증했다. current kernel과 RP2040 firmware의
> build 및 validation initramfs boot smoke는 통과했지만 실제 RP2040 full chain은 아직
> 검증되지 않았다.

### Stage 1 — USB core short-configuration leak

- 구현: short configuration response에 의한 uninitialized heap disclosure
- 위치: `usb/core/message.c`, `usb/core/config.c`
- 입력 조건:
  - 첫 configuration header의 `wTotalLength`는 공격자가 원하는 kmalloc
    cache 크기를 광고한다.
  - 두 번째 `GET_DESCRIPTOR(CONFIGURATION)` 응답은 정상 configuration, interface 및
    bulk IN endpoint descriptor까지만 반환하는 25-byte short response다.
- 데이터 흐름:
  - backing buffer는 광고된 길이로 `kmalloc(..., GFP_KERNEL_ACCOUNT)` 된다.
    이 flag는 userspace pipe ring의 `GFP_KERNEL_ACCOUNT` allocation과 같은
    kmalloc cache를 선택하기 위한 의도된 challenge 변경이다.
  - parser에는 실제 수신 길이만 전달되어 미수신 tail을 해석하지 않는다.
  - raw descriptor reader에는 광고 길이가 보존되어 sysfs
    `.../descriptors` 또는 usbfs read가 미수신 tail을 userspace로 복사한다.
- 보안상 결과: 동일 kmalloc cache의 잔여 바이트를 유출할 수 있다.
- 안정성 의도: parser가 tail에 진입하지 않으므로 short response 뒤에도 정상
  interface가 생성되고 driver matching까지 진행할 수 있어야 한다.
- 현재 검증 수준:
  - host-side poisoned-allocation model에서 canonical kernel pointer 유출: PASS
  - QEMU Linux 6.8.12, KASAN, `slub_debug=FZ`, `dummy_hcd`와 Raw Gadget에서
    short response 뒤 enumeration 및 sysfs disclosure: PASS
  - RP2040 enumeration: NOT TESTED
- 유효 정보 조건: 반복 heap grooming으로 함수 포인터 또는 vmemmap 주소를
  포함한 동일 크기 객체를 재사용시켜 KASLR slide나 heap 주소를 계산할 수 있어야
  한다. 단순 nonzero tail만으로 Stage 1 성공을 확정하지 않는다.
- 확인된 grooming 경로:
  - 16-entry `pipe_buffer` ring은 x86_64에서 640바이트를 요청해 kmalloc-1k를
    사용한다. 여러 entry를 page-sized write로 채운 뒤 pipe를 해제하면 entry 1
    이후의 `struct page *`가 남는다. entry 0 일부는 25-byte configuration response가
    덮는다.
  - `/proc/self/stat`의 `single_open()`은 32-byte `seq_operations`를
    `GFP_KERNEL_ACCOUNT`로 할당한다. 해제 뒤 32바이트 configuration allocation으로
    재사용하면 마지막 `show` 함수 포인터인 `proc_single_show`의 상위 7바이트가
    남는다. 하위 1바이트는 알려진 symbol offset으로 복원해 KASLR slide를 계산한다.
    QEMU-only emulator는 allocator 변동에 대비해 같은 32-byte configuration을 8개
    제공하며 참가자는 유효한 aligned runtime pointer를 선택한다.
  - pipe 해제 시 `ops`는 NULL이 되므로 freed pipe ring 자체에서는
    `anon_pipe_buf_ops`를 얻지 못한다.
  - pipe resize는 live entry를 새 ring으로 복사한 뒤 old ring을 해제한다. QEMU-only
    target은 이 old ring을 `kfree_sensitive()`로 초기화해 helper page와 runtime ops를
    동시에 남기는 우회를 차단한다. live entry와 정상 close 경로는 바꾸지 않는다.
  - root-owned SUID helper의 executable page를 pipe ring의 세 번째 entry에 `splice()`하고
    alternating ring hole을 8개 configuration allocation으로 재사용하면, canonical,
    64-byte-aligned vmemmap 후보가 configuration offset `0x50`에 반복해서 남는다. harness는
    target의 LA57/KASLR vmemmap 1 GiB 정렬 구간에서 2 GiB RAM에 필요한 32 MiB 범위 안에 있고 두
    configuration 이상에서 같은 후보만 사용한다. 이 값을 사용한 page-cache overwrite와
    helper 실행을 target-equivalent QEMU에서 10/10회 확인했다.
    helper source fd를 닫은 별도 live pipe도 10초 뒤 executable-page byte를 반환해 page
    reference가 유지됨을 확인했다.

### Stage 2 — Custom driver

- driver binding 진입점: `custom_probe()`
- 취약점 trigger: `/dev/kapo_ctf0`의 `KAPO_FETCH_RECORDS` ioctl
- 매칭 조건: vendor-specific interface class `0xff`, subclass `0x42`,
  protocol `0x01`
- protocol:
  - vendor control request로 little-endian 16-bit `record_count`를 읽는다.
  - driver는 record 배열을 할당하면서 element가 아니라 record 포인터 자체의 크기를
    사용해 `kmalloc_array(record_count, sizeof(records), GFP_KERNEL_ACCOUNT)`로
    할당한다.
  - bulk IN endpoint에서는 실제 16-byte record 배열 크기로
    `record_count * sizeof(struct kapo_record)` 바이트를 같은 buffer로 수신한다.
  - 방향 1의 `record_count = 128`은 1024-byte request와 2048-byte receive를 유지한다.
  - 방향 2 trusted emulator의 `record_count = 65`는 520-byte request가 kmalloc-1k
    object를 받고 1040 bytes를 수신한다. 따라서 다음 kmalloc-1k object의 처음 16 bytes,
    즉 `pipe_buffer.page`, `offset`, `len`만 overwrite한다.
- trigger timing:
  - `probe()`는 bulk endpoint를 확인하고 userspace device node만 등록한다.
  - 참가자는 Stage 1 sysfs leak을 읽어 KASLR slide와 `struct page *`를 얻는다.
  - 참가자가 live pipe rings를 grooming한 뒤 driver ioctl을 호출하면 metadata read,
    vulnerable allocation, bulk receive가 실행된다. 이 순서로 Stage 1 관찰 이전에
    Stage 2가 자동 실행되는 문제를 피한다.
- Stage 1 연결:
  - leaked `proc_single_show`에서 KASLR slide와 `page_cache_pipe_buf_ops` 주소를
    계산한다.
  - 최종 대상으로 사용할 SUID helper의 file-backed page를 pipe entry에 넣고,
    해제된 ring에서 해당 `struct page *`를 Stage 1로 유출한다. helper 전용
    grooming은 target-equivalent QEMU에서 확인했다.
  - 방향 2 participant는 1-byte anonymous pipe buffer를 live target으로 준비한다.
    partial overflow는 `page`를 leaked helper page로, `offset`을 0으로, `len`을 helper
    patch offset으로 바꾼다. 기존 anonymous `ops`, `PIPE_BUF_FLAG_CAN_MERGE`, `private`는
    overwrite 범위 밖에 있어 반드시 live entry에서 보존해야 한다.
  - driver의 exact runtime `page_cache_pipe_buf_ops` context 검증은 seq KASLR leak을
    계속 요구하지만, QEMU partial payload가 실제 live entry에 기록하는 ops는 보존된
    `anon_pipe_buf_ops`다.
  - 이후 pipe write를 외부 trigger로 사용해 SUID helper의 page cache를 덮어쓴다.
- 검증 분리:
  - KASAN build에서는 1024-byte allocation 경계를 넘는 bulk receive report를 확인한다.
  - SLUB redzone 때문에 실제 인접 object 도달이 달라질 수 있으므로 adjacency와 pipe
    primitive는 debug redzone 없는 target-equivalent build에서 별도로 확인한다.
- 현재 검증 수준:
  - 방향 2 `record_count = 65`, 520-byte request, 1040-byte receive와 16-byte adjacent
    overwrite: PASS.
  - 방향 2 partial overwrite가 live anonymous entry의 `ops/flags/private`를 보존한 채
    helper page-cache write, UID 0, test flag를 달성: TCG cold boot 5/5 PASS.
  - 이전 full-entry exploit은 partial 후보에서 1040-byte receive 뒤 flag 획득 실패: PASS.
  - 단서 완화 후 object에서 `record_count = 128`의 1024-byte allocation과
    2048-byte receive 길이 유지: PASS
  - 단서 완화 전 ioctl을 통한 외부 trigger와 2048-byte 반환: PASS
  - 단서 완화 전 KASAN `slab-out-of-bounds` write report: PASS
  - 단서 완화 전 SLUB `Right Redzone overwritten` report: PASS
  - 단서 완화 전 redzone-free single-CPU trace에서 freed pipe ring 주소를 vulnerable
    allocation이 정확히 재사용하고, 바로 높은 주소의 live pipe ring까지 write가 도달:
    PASS
  - 단서 완화 후 QEMU에서 파일 payload를 사용한 default/seq Stage 2 ioctl의
    2048-byte 반환 및 KASAN write report: PASS
  - target-equivalent Linux 6.8.12 build headers를 컴파일해 산출한
    `struct pipe_buffer` layout: PASS. 크기는 40 bytes이고 `page`, `offset`,
    `len`, `ops`, `flags`, `private`의 byte offset은 각각 0, 8, 12, 16, 24,
    32다.
  - 1024-byte allocation prefix를 zero로 유지하고 바로 뒤 entry 하나를
    little-endian으로 재구성하는 2048-byte payload 생성과 자체 검사: PASS.
    page/ops/offset은 실행 시 입력이며 binary payload와 실제 주소는 저장소에
    포함하지 않는다.
  - 생성 payload를 Raw Gadget file-payload 경로로 전달한 QEMU Stage 2 ioctl의
    2048-byte 반환과 KASAN allocation-boundary report: PASS. 이 전달 검증은 live
    entry의 reference lifetime이나 page-cache write를 검증하지 않는다.
  - SUID helper의 file-backed page pointer Stage 1 leak: PASS. 유출한 값을 사용한
    page-cache overwrite가 target-equivalent QEMU full chain에서 10/10회 helper의
    지정된 executable page에 도달했다.
  - live pipe entry의 helper page reference: PASS (source fd close 뒤 10초 보존)
  - guest-to-device transfer context, controlled pipe metadata 및 page-cache write: PASS
  - 비특권 UID에서 변조한 SUID helper를 실행해 UID 0 및 test flag 획득: PASS
    (target-equivalent QEMU cold boot 10/10)
- 상태: QEMU full chain은 Confirmed. 실제 RP2040 full chain은 NOT TESTED.

## 7. 검증할 가설

- [x] host model에서 short response의 미초기화 tail이 raw descriptor 출력으로
  전달된다.
- [x] QEMU Linux 6.8.12에서 trace로 640-byte pipe ring과 1024-byte USB
  configuration이 같은 kmalloc-1k cache를 사용하고, USB allocation이 해제된 pipe
  ring의 정확한 주소를 재사용함을 확인했다.
- [x] 여러 pipe entries를 채운 뒤 configuration offset `0x28`부터 canonical
  vmemmap `struct page *` 후보가 유출된다. 5/5회 확인했다.
- [x] 32-byte `seq_operations` grooming으로 `proc_single_show`의 상위 7바이트가
  configuration offset `0x19`에 유출된다. 5/5회 확인했다.
- [x] resize로 해제된 live pipe ring의 page+ops 동시 유출은 old ring 초기화 후 기존
  독립 shortcut exploit 3/3에서 차단되고, 의도한 page-only + seq leak full chain은
  강화 artifact cold boot 5/5를 통과했다.
- [x] Stage 2 context의 ops를 runtime `page_cache_pipe_buf_ops`로 제한했다. 0x40 낮은
  대체 pipe ops를 넣은 회귀 실행은 seq leak 뒤 `EINVAL`로 거부됐다.
- [x] sysfs `descriptors`에서 광고 길이 1024바이트의 configuration tail을
  외부에서 읽을 수 있다.
- [x] QEMU `dummy_hcd`에서 Stage 1 이후 vendor interface가 생성된다.
- [x] QEMU Raw Gadget의 `ff/42/01` interface가 `kapo_ctf` driver에 매칭되어
  `custom_probe()`가 실행된다.
- [x] Stage 1에서 얻은 정보나 상태를 Stage 2에서 이용할 수 있다.
- [x] Stage 2 ioctl이 1024-byte allocation에 2048-byte bulk payload를 기록해
  KASAN 및 SLUB에서 allocation 경계 초과를 재현한다.
- [x] redzone-free QEMU trace에서 Stage 2 allocation이 controlled pipe hole의 정확한
  주소를 재사용하고 인접 live pipe ring까지 도달한다.
- [x] Stage 1의 canonical vmemmap 후보가 SUID helper의 file-backed page pointer임을
  독립적으로 식별할 수 있다.
- [x] guest가 Stage 1의 동적 `page`와 `ops`를 USB device에 전달하고 같은 연결의
  Stage 2 bulk payload에 반영할 수 있다.
- [x] `page_cache_pipe_buf_ops`와 `PIPE_BUF_FLAG_CAN_MERGE`로 재구성한 pipe entry가
  helper의 page cache를 덮어쓴다.
- [x] QEMU-only 16-byte overflow로 `page/offset/len`만 바꾸고 live anonymous entry의
  `anon_pipe_buf_ops`, `PIPE_BUF_FLAG_CAN_MERGE`, `private`를 보존해 helper page cache를
  덮어쓴다. cold boot 5/5로 확인했다.
- [x] 변조한 SUID helper 실행으로 UID 0을 획득하고 `KAPO{test_flag}`를 읽는다.
- [ ] 32-byte와 1024-byte configuration을 한 장치의 두 configuration으로 제공해
  한 번의 연결에서 두 leak을 얻을 수 있다.
- [x] target-equivalent QEMU에서 두 primitive로 연속 10회 권한 상승할 수 있다.
- [ ] 실제 RP2040에서 두 primitive로 안정적인 권한 상승이 가능하다.

## 8. 최종 권한 상승 설계

### 설계 결정

- 최종 primitive는 범용 kernel ROP나 arbitrary kernel write가 아니라,
  `pipe_buffer`를 이용한 file page-cache write로 고정한다.
- 최종 대상은 참가자 환경에 포함된 전용 root-owned SUID helper다.
- helper는 입력을 처리하지 않고 평상시에는 고정 메시지만 출력한다. 참가자는
  executable page의 지정된 함수 부분을 root shell payload로 바꾼 뒤 helper를
  실행한다.
- helper가 UID 0으로 실행된 뒤 `/flag`를 읽는 것을 최종 성공 조건으로 한다.
- 설계 상태: QEMU runtime 검증 완료. helper page identity, controlled page-cache write와
  권한 상승을 target-equivalent cold boot에서 10/10회 확인했다. RP2040은 NOT TESTED.

### 의도된 pipe metadata

방향 1의 Stage 2 overflow는 인접 live pipe ring의 활성 entry 전체를 다음 의미로
재구성한다. target
Linux 6.8.12 build에서 `struct pipe_buffer` 크기는 40 bytes이며 field offset은
`page=0`, `offset=8`, `len=12`, `ops=16`, `flags=24`, `private=32`로 재현 가능하게
확인했다. `scripts/verify-pipe-buffer-layout.sh <linux-source> <linux-output>`는 target
header와 generated configuration을 컴파일해 이 값을 출력한다. 이 스크립트는 source와
output이 모두 Linux 6.8.12이고 output의 `source` link가 입력 source와 일치하는지
검사한 뒤 target `.config` SHA-256와 일치해야 통과한다.

```text
page    = Stage 1에서 유출한 SUID helper page의 struct page 주소
offset  = helper patch offset - 1
len     = 1
ops     = KASLR slide를 적용한 page_cache_pipe_buf_ops 주소
flags   = PIPE_BUF_FLAG_CAN_MERGE
private = 0
```

helper의 patch offset 바로 앞 1바이트를 pipe에 `splice()`해 유효한 page reference와
활성 entry를 만든다. Stage 2 payload가 해당 entry를 재구성한 뒤 pipe에 쓰면
`offset + len`부터 helper의 page cache가 바뀐다. 이 동작과 reference lifetime,
page boundary 제약은 target-equivalent QEMU에서 검증했다. payload generator는
allocation의 첫 1024 bytes와 entry 뒤 tail을 zero로 유지하고 byte 1024의 entry만
채운다. `page`, `ops`, `offset`은 `scripts/make-stage2-pipe-payload.c`에 runtime input으로
전달한다. full-chain harness는 같은 layout을 USB device에서 구성하며 live entry
재구성과 page-cache write를 10/10회 확인했다. `len=1`이 page를 넘지 않도록 offset을
0..4094로 제한한다.

방향 2 QEMU-only partial overwrite는 다음 16 bytes만 제공한다.

```text
page   = Stage 1에서 유출한 SUID helper page의 struct page 주소
offset = 0
len    = helper page 안의 patch offset
```

대상 live entry는 participant가 1 byte를 쓴 anonymous pipe buffer여야 한다. 따라서
byte 16 이후의 `anon_pipe_buf_ops`, `PIPE_BUF_FLAG_CAN_MERGE`, `private = 0`가 기존
entry에서 보존된다. transfer context의 runtime `page_cache_pipe_buf_ops`는 driver의
KASLR 검증에 사용되지만 partial USB payload에는 복사되지 않는다.

### Stage 1 leak 구성

- kmalloc-32 grooming은 기존처럼 해제된 `seq_operations`에서
  `proc_single_show`의 상위 7바이트를 얻는 데 사용한다.
  organizer harness는 readiness FIFO를 grooming 전에 열고, 512개 중 마지막 128개만
  연속 해제한 직후 `seq`를 기록한다. QEMU-only emulator는 동일한 32-byte
  configuration을 8개 제공해 allocator 변동 속에서도 유효한 tail을 선택하게 한다.
  hardware mode는 기존 1개 configuration contract를 유지한다.
- kmalloc-1k grooming은 일반 anonymous pipe page가 아니라 SUID helper의 file-backed
  page가 세 번째 `pipe_buffer` entry에 남도록 구성한다. 앞의 두 entries는 25-byte USB
  response가 덮는 영역을 피하기 위한 padding이다. 400개 ring 중 홀수 ring을 해제하고
  1024-byte configuration 8개로 재할당해 configuration offset `0x50`의 canonical,
  64-byte-aligned 후보를 찾는다. target vmemmap의 base alignment와 2 GiB RAM 범위를
  적용하고 두 configuration 이상에 반복된 값만 사용한다. 이 후보로 helper page cache를
  실제 변조해 page identity를 target-equivalent QEMU 10/10회 확인했다.
- 참가자 firmware는 우선 기존처럼 32-byte와 1024-byte mode를 나눠 구현한다.
  구현된 firmware는 helper 1024-byte mode, seq 32-byte mode, Stage 2 1024-byte mode를
  순서대로 제공한다. guest가 custom driver의 `KAPO_NEXT_STAGE` ioctl을 호출하면 vendor
  request `0x5c`가 전달되고, firmware가 2초 disconnect 후 다음 mode로 reconnect한다.
  helper와 seq heap grooming은 각 enumeration 직전에 guest harness가 수행한다.
  안정화 후 `bNumConfigurations = 2`로 두 short configuration을 한 번의 enumeration에
  제공하는 방식은 선택적으로 시험할 수 있다. 이 단일 연결 방식은 Unverified다.

## 9. 방향 1 참가자 배포 및 검증 계획

이 절은 RP2040 hardware edition 전용이다. 현재 우선순위인 방향 2의 배포,
신뢰 경계와 완료 기준은 §0을 따른다.

### 배포 방향

- 난이도와 실행 안정성을 위해 `kapo_ctf` custom driver는 최종 커널에 built-in으로
  포함한다. 독립된 `customdriver.ko`는 참가자에게 제공하지 않는다.
- 참가자에게는 exact target의 `bzImage`, symbol을 유지하고 DWARF를 제거한 `vmlinux`,
  `System.map`, kernel config, Ubuntu 24.04.4 initramfs, QEMU 실행 스크립트, SUID helper의
  분석용 사본, RP2040 firmware template과 `SHA256SUMS`를 제공한다.
- 이 파일은 `participant-release/`에만 모은다. 완성 exploit, internal harness와 실제로
  동작하는 attack firmware는 포함하지 않는다.
- 내부 하드웨어 검증용으로 위 kernel artifact, test initramfs와 RP2040 UF2를 묶은
  `dist/hardware-validation/`을 생성했다. 실제 장치 검증 전이므로 참가자 최종 배포본은
  아니다.
- rootfs에는 `KAPO{test_flag}`를 담은 root-owned mode `0400`의 `/flag`와 root-owned
  mode `4755`의 SUID helper를 둔다. `/dev/kapo_ctf0`은 일반 참가자 계정에서 접근
  가능해야 한다.
- target build는 KASLR, SMEP, SMAP을 켜고 KASAN과 SLUB redzone debugging을 끈다.
  QEMU는 우선 1 vCPU와 고정 메모리로 검증한다.
- 실제 flag와 server-only initramfs는 참가자 배포본과 저장소에 포함하지 않는다.

### 참가자에게 제공하지 않는 자료

- 이 문서, 공식 solve와 exploit
- `STATUS.md`, `HANDOFF.md`
- Raw Gadget trigger, heap grooming 및 adjacency 검증 스크립트
- KASAN/SLUB 내부 검증 로그
- 실제 flag, 운영 이미지, 자격 증명

### 방향 1 ticket 검증 입력

참가자는 RP2040 firmware image와 source, guest exploit binary와 source, build/run 절차,
각 파일의 hash를 제출한다. 운영진은 disposable 검증 VM에서 동일한 kernel과 rootfs를
사용하되 `/flag` 값만 교체해 검증한다. 참가자 프로그램은 host가 아니라 guest의
비특권 계정에서 실행한다.

### 완료 기준

- [x] release 전 내부 기준으로 target-equivalent QEMU cold boot에서 연속 10회 UID 0 및
  test flag 획득
- 실제 RP2040과 USB passthrough 환경에서 연속 10회 성공
- 매 boot의 KASLR 주소가 달라도 Stage 1 leak을 이용해 성공
- 배포 환경과 ticket 환경의 kernel, config, driver, helper hash가 동일하고 flag만 다름

## 10. 미정 사항

- RP2040 disconnect/reconnect를 포함한 QEMU USB passthrough 방식의 실제 장치 검증
- 현재 Ubuntu 24.04.4 initramfs와 9p `/work` exploit 반입 방식의 현장 확정
- 두 configuration을 이용한 단일 enumeration leak의 재현 가능성
- 최종 kernel config와 release artifact hash
- 실제 flag 검증 절차
- ticket 현장 검증의 반복 횟수와 합격 기준

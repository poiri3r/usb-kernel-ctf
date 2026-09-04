# USB Kernel CTF Challenge Specification

> 이 문서는 출제자 및 개발 에이전트 전용이다.
> 참가자 배포본에 포함하지 않는다.

## 1. 문제 개요

- 문제 이름: TBD
- 분야: Pwn / Kernel / Hardware
- 목표 난이도: 상
- 실행 환경: Ubuntu 24.04 + Linux Kernel 6.8
- 공격 입력 장치: RP2040 Zero
- 플래그 형식: `KAPO{...}`

### 한 줄 설명

RP2040으로 제작한 USB 장치를 이용해 취약한 Linux USB 커널 드라이버를 공격하고, 최종적으로 플래그를 읽는 문제다.

이 문제는 Linux USB core의 configuration descriptor 파싱 취약점과 CTF용 custom USB driver의 취약점을 순차적으로 연결한다. RP2040이 조작된 configuration descriptor를 전달하면 `usb/core/config.c`에서 첫 번째 primitive가 발생한다. 이후 USB enumeration이 계속 진행되고, 특정 device/interface 조건이 충족되면 custom driver의 `probe()`가 실행되어 두 번째 primitive로 연결된다.

## 2. 출제 의도
이 문제를 통해 참가자가 경험해야 하는 핵심 내용은 다음과 같다

- Linux USB 드라이버가 장치를 인식하는 일련의 과정
- `probe()`와 `device_add()` 및 USB 요청 처리 과정
- 커널 드라이버에서 발생하는 메모리 취약점
- 제한된 primitive를 exploit chain으로 발전시키는 과정
- RP2040 펌웨어를 이용한 공격 입력 생성

## 3. 문제 운영 및 검증 방식

참가자는 KAIST와 포항공대 학생 각 11명으로 구성된 총 2팀이며, 팀별로 서로 다른 방에 배정된다.

참가자는 로컬 환경에서 exploit을 개발하여 `KAPO{test_flag}`를 획득한 후 운영진에게 ticket을 요청할 수 있다. 운영진은 해당 방에 검증용 노트북을 가져가 exploit primitive의 안정성과 실제 flag 획득 가능 여부를 테스트한다.

## 4. 시스템 구성
### Host

- OS: Ubuntu 24.04
- Kernel: Linux 6.8.x
- Architecture: x86_64
- 실행 방식: VM
- initramfs 사용 여부: TBD

### USB device

- Board: RP2040 Zero

### Kernel code

- 원본 USB 코드: `usb/`
- custom driver: `usb/class/customdriver.c`

## 5. 현재 확정된 단계

- 공격 입력은 RP2040 Zero에서 전달된다.
- Linux USB core의 configuration descriptor 처리 경로에 Stage 1 취약점을 구현한다.
- 첫 번째 취약점은 custom driver의 `probe()` 이전에 발생한다.
- 특정 장치/interface/descriptor 조건을 만족하면 custom driver가 매칭된다.
- `probe()`에서 추가 primitive로 연결한다.
- 최종 목표는 커널 권한 획득 및 flag 읽기다.

## 6. Exploit Chain

> Stage 1 leak과 변경 전 Stage 2 overflow 및 인접 live object 도달은 Confirmed다.
> source-level 단서 완화 후에는 object의 크기 불일치만 재검증했고 QEMU runtime
> regression은 `NOT TESTED`다. 아래에서 `NOT TESTED` 또는 `Hypothesis`로 표시한
> page-cache primitive와 최종 권한 상승도 아직 검증되지 않았다.

### Stage 1 — USB core short-configuration leak

- 구현: short configuration response에 의한 uninitialized heap disclosure
- 위치: `usb/core/message.c`, `usb/core/config.c`
- 입력 조건:
  - 첫 configuration header의 `wTotalLength`는 공격자가 원하는 kmalloc
    cache 크기를 광고한다.
  - 두 번째 `GET_DESCRIPTOR(CONFIGURATION)` 응답은 정상 configuration 및
    interface descriptor까지만 반환하는 short response다.
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
  - pipe 해제 시 `ops`는 NULL이 되므로 freed pipe ring 자체에서는
    `anon_pipe_buf_ops`를 얻지 못한다.

### Stage 2 — Custom driver

- 진입점: `custom_probe()`
- 매칭 조건: vendor-specific interface class `0xff`, subclass `0x42`,
  protocol `0x01`
- protocol:
  - vendor control request로 little-endian 16-bit `record_count`를 읽는다.
  - driver는 record 배열을 할당하면서 element가 아니라 record 포인터 자체의 크기를
    사용해 `kmalloc_array(record_count, sizeof(records), GFP_KERNEL_ACCOUNT)`로
    할당한다.
  - bulk IN endpoint에서는 실제 16-byte record 배열 크기로
    `record_count * sizeof(struct kapo_record)` 바이트를 같은 buffer로 수신한다.
  - `record_count = 128`이면 allocation request는 1024바이트, receive length는
    2048바이트다. 기본 16-entry pipe ring의 640-byte request와 같은 kmalloc-1k
    cache에 들어간다.
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
    해제된 ring에서 해당 `struct page *`를 Stage 1로 유출한다. 이 target-specific
    leak은 아직 검증하지 않았다.
  - overflow payload는 인접 live `pipe_buffer`의 `page`, `offset`, `len`, `ops`,
    `flags`, `private`를 일관된 값으로 재구성한다.
  - `PIPE_BUF_FLAG_CAN_MERGE`를 설정한 뒤 pipe write를 외부 trigger로 사용해
    SUID helper의 page cache를 덮어쓴다.
- 검증 분리:
  - KASAN build에서는 1024-byte allocation 경계를 넘는 bulk receive report를 확인한다.
  - SLUB redzone 때문에 실제 인접 object 도달이 달라질 수 있으므로 adjacency와 pipe
    primitive는 debug redzone 없는 target-equivalent build에서 별도로 확인한다.
- 현재 검증 수준:
  - 단서 완화 후 object에서 `record_count = 128`의 1024-byte allocation과
    2048-byte receive 길이 유지: PASS
  - 단서 완화 전 ioctl을 통한 외부 trigger와 2048-byte 반환: PASS
  - 단서 완화 전 KASAN `slab-out-of-bounds` write report: PASS
  - 단서 완화 전 SLUB `Right Redzone overwritten` report: PASS
  - 단서 완화 전 redzone-free single-CPU trace에서 freed pipe ring 주소를 vulnerable
    allocation이 정확히 재사용하고, 바로 높은 주소의 live pipe ring까지 write가 도달:
    PASS
  - 단서 완화 후 QEMU Stage 2 runtime regression: NOT TESTED
  - controlled pipe metadata 및 page-cache write: NOT TESTED
- 상태: overflow와 live pipe target 도달은 Confirmed. page primitive와 최종 exploit
  연결은 Hypothesis.

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
- [x] sysfs `descriptors`에서 광고 길이 1024바이트의 configuration tail을
  외부에서 읽을 수 있다.
- [x] QEMU `dummy_hcd`에서 Stage 1 이후 vendor interface가 생성된다.
- [x] QEMU Raw Gadget의 `ff/42/01` interface가 `kapo_ctf` driver에 매칭되어
  `custom_probe()`가 실행된다.
- [ ] Stage 1에서 얻은 정보나 상태를 Stage 2에서 이용할 수 있다.
- [x] Stage 2 ioctl이 1024-byte allocation에 2048-byte bulk payload를 기록해
  KASAN 및 SLUB에서 allocation 경계 초과를 재현한다.
- [x] redzone-free QEMU trace에서 Stage 2 allocation이 controlled pipe hole의 정확한
  주소를 재사용하고 인접 live pipe ring까지 도달한다.
- [ ] SUID helper의 file-backed page pointer를 Stage 1에서 안정적으로 유출할 수 있다.
- [ ] `page_cache_pipe_buf_ops`와 `PIPE_BUF_FLAG_CAN_MERGE`로 재구성한 pipe entry가
  helper의 page cache를 덮어쓴다.
- [ ] 변조한 SUID helper 실행으로 UID 0을 획득하고 `KAPO{test_flag}`를 읽는다.
- [ ] 32-byte와 1024-byte configuration을 한 장치의 두 configuration으로 제공해
  한 번의 연결에서 두 leak을 얻을 수 있다.
- [ ] 두 primitive로 안정적인 권한 상승이 가능하다.

## 8. 최종 권한 상승 설계

### 설계 결정

- 최종 primitive는 범용 kernel ROP나 arbitrary kernel write가 아니라,
  `pipe_buffer`를 이용한 file page-cache write로 고정한다.
- 최종 대상은 참가자 환경에 포함된 전용 root-owned SUID helper다.
- helper는 입력을 처리하지 않고 평상시에는 고정 메시지만 출력한다. 참가자는
  executable page의 지정된 함수 부분을 root shell payload로 바꾼 뒤 helper를
  실행한다.
- helper가 UID 0으로 실행된 뒤 `/flag`를 읽는 것을 최종 성공 조건으로 한다.
- 설계 상태: 선택 완료. runtime 검증 상태: NOT TESTED.

### 의도된 pipe metadata

Stage 2 overflow로 인접 live pipe ring의 활성 entry를 다음 의미로 재구성한다. 실제
필드 offset과 symbol offset은 최종 Linux 6.8.x build를 기준으로 산출한다.

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
`offset + len`부터 helper의 page cache가 바뀌어야 한다. 이 동작과 reference lifetime,
page boundary 제약은 target-equivalent QEMU에서 별도로 검증한다.

### Stage 1 leak 구성

- kmalloc-32 grooming은 기존처럼 해제된 `seq_operations`에서
  `proc_single_show`의 상위 7바이트를 얻는 데 사용한다.
- kmalloc-1k grooming은 일반 anonymous pipe page가 아니라 SUID helper의 file-backed
  page가 두 번째 `pipe_buffer` entry에 남도록 변경한다. 첫 entry는 25-byte USB
  response가 덮는 영역을 피하기 위한 padding 역할을 한다.
- 참가자 firmware는 우선 기존처럼 32-byte와 1024-byte mode를 나눠 구현한다.
  안정화 후 `bNumConfigurations = 2`로 두 short configuration을 한 번의 enumeration에
  제공하는 방식을 시험한다. 이 단일 연결 방식은 Unverified다.

## 9. 참가자 배포 및 검증 계획

### 배포 방향

- 난이도와 실행 안정성을 위해 `kapo_ctf` custom driver는 최종 커널에 built-in으로
  포함한다. 독립된 `customdriver.ko`는 참가자에게 제공하지 않는다.
- 참가자에게는 exact target의 `bzImage`, symbol을 유지하고 DWARF를 제거한 `vmlinux`,
  `System.map`, kernel config, Ubuntu 24.04 rootfs, QEMU 실행 스크립트, SUID helper의
  분석용 사본, RP2040 firmware template과 `SHA256SUMS`를 제공한다.
- rootfs에는 `KAPO{test_flag}`를 담은 root-owned mode `0400`의 `/flag`와 root-owned
  mode `4755`의 SUID helper를 둔다. `/dev/kapo_ctf0`은 일반 참가자 계정에서 접근
  가능해야 한다.
- target build는 KASLR, SMEP, SMAP을 켜고 KASAN과 SLUB redzone debugging을 끈다.
  QEMU는 우선 1 vCPU와 고정 메모리로 검증한다.
- 실제 flag와 운영진 검증 이미지는 참가자 배포본과 저장소에 포함하지 않는다.

### 참가자에게 제공하지 않는 자료

- 이 문서, 공식 solve와 exploit
- `STATUS.md`, `HANDOFF.md`
- Raw Gadget trigger, heap grooming 및 adjacency 검증 스크립트
- KASAN/SLUB 내부 검증 로그
- 실제 flag, 운영 이미지, 자격 증명

### ticket 검증 입력

참가자는 RP2040 firmware image와 source, guest exploit binary와 source, build/run 절차,
각 파일의 hash를 제출한다. 운영진은 disposable 검증 VM에서 동일한 kernel과 rootfs를
사용하되 `/flag` 값만 교체해 검증한다. 참가자 프로그램은 host가 아니라 guest의
비특권 계정에서 실행한다.

### 완료 기준

- release 전 내부 기준으로 target-equivalent QEMU cold boot에서 연속 10회 UID 0 및
  test flag 획득
- 실제 RP2040과 USB passthrough 환경에서 연속 10회 성공
- 매 boot의 KASLR 주소가 달라도 Stage 1 leak을 이용해 성공
- 배포 환경과 ticket 환경의 kernel, config, driver, helper hash가 동일하고 flag만 다름

## 10. 미정 사항

- exact QEMU 실행 옵션과 RP2040 USB passthrough 방식
- initramfs 사용 여부
- Ubuntu 24.04 rootfs의 이미지 형식과 exploit 반입 방식
- SUID helper의 binary layout, patch offset과 payload 크기
- 두 configuration을 이용한 단일 enumeration leak의 재현 가능성
- 최종 kernel config와 release artifact hash
- 실제 flag 검증 절차
- ticket 현장 검증의 반복 횟수와 합격 기준

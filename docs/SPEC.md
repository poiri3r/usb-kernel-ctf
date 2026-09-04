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

## 6. 검토 중인 Exploit Chain

> 아래 내용은 아직 확정된 설계가 아니라 구현 가능성을 검증 중인 가설이다.

### Stage 1 — USB core short-configuration leak

- 구현 후보: short configuration response에 의한 uninitialized heap disclosure
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
    이후의 `struct page *`가 남는다. entry 0은 18-byte USB response가 덮는다.
  - `/proc/self/stat`의 `single_open()`은 32-byte `seq_operations`를
    `GFP_KERNEL_ACCOUNT`로 할당한다. 해제 뒤 32바이트 configuration allocation으로
    재사용하면 마지막 `show` 함수 포인터인 `proc_single_show`의 상위 7바이트가
    남는다. 하위 1바이트는 알려진 symbol offset으로 복원해 KASLR slide를 계산한다.
  - pipe 해제 시 `ops`는 NULL이 되므로 freed pipe ring 자체에서는
    `anon_pipe_buf_ops`를 얻지 못한다.

### Stage 2 — Custom driver

- 진입점: `custom_probe()`
- 매칭 조건 후보: vendor-specific interface class `0xff`, subclass `0x42`,
  protocol `0x01`
- 제안 protocol:
  - vendor control request로 little-endian 16-bit `record_count`를 읽는다.
  - driver는 record pointer table이라고 잘못 해석해
    `kmalloc_array(record_count, sizeof(void *), GFP_KERNEL_ACCOUNT)`로 할당한다.
  - bulk IN endpoint에서는 실제 16-byte record 배열이라고 해석해
    `record_count * sizeof(struct kapo_record)` 바이트를 같은 buffer로 수신한다.
  - `record_count = 128`이면 allocation request는 1024바이트, receive length는
    2048바이트다. 기본 16-entry pipe ring의 640-byte request와 같은 kmalloc-1k
    cache에 들어간다.
- trigger timing:
  - `probe()`는 bulk endpoint를 확인하고 userspace device node만 등록한다.
  - 참가자는 Stage 1 sysfs leak을 읽어 KASLR slide와 vmemmap page pointer를 얻는다.
  - 참가자가 live pipe rings를 grooming한 뒤 driver ioctl을 호출하면 metadata read,
    vulnerable allocation, bulk receive가 실행된다. 이 순서로 Stage 1 관찰 이전에
    Stage 2가 자동 실행되는 문제를 피한다.
- Stage 1 연결:
  - leaked `proc_single_show`에서 KASLR slide와 `anon_pipe_buf_ops` 주소를 계산한다.
  - leaked `struct page *`는 vmemmap에서 page 단위 산술의 기준으로 사용한다.
  - overflow payload는 인접 live `pipe_buffer`의 `page`, `offset`, `len`, `ops`,
    `flags`, `private`를 일관된 값으로 재구성한다.
  - 이후 pipe read/write를 외부 trigger로 사용해 page-oriented primitive를 검증한다.
- 검증 분리:
  - KASAN build에서는 1024-byte allocation 경계를 넘는 bulk receive report를 확인한다.
  - SLUB redzone 때문에 실제 인접 object 도달이 달라질 수 있으므로 adjacency와 pipe
    primitive는 debug redzone 없는 target-equivalent build에서 별도로 확인한다.
- 현재 검증 수준:
  - `record_count = 128`에서 1024-byte kmalloc-1k allocation과 2048-byte bulk
    receive 발생: PASS
  - ioctl을 통한 외부 trigger와 2048-byte 반환: PASS
  - KASAN `slab-out-of-bounds` write report: PASS
  - SLUB `Right Redzone overwritten` report: PASS
  - redzone-free single-CPU trace에서 freed pipe ring 주소를 vulnerable allocation이
    정확히 재사용하고, 바로 높은 주소의 live pipe ring까지 write가 도달: PASS
  - controlled pipe metadata 및 page-oriented read/write: NOT TESTED
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
- [ ] 두 primitive로 안정적인 권한 상승이 가능하다.

## 8. 미정 사항

- 사용할 VM 또는 hypervisor
- VM의 USB passthrough 방식
- initramfs 사용 여부
- Stage 1에서 조작할 정확한 descriptor 필드
- 정보 유출 경로
- custom driver 매칭 조건
- Stage 2 취약점 유형
- 공격 대상 kernel object
- SMEP/SMAP/KASLR 대응 방법
- 실제 flag 검증 절차
- 참가자가 운영진에게 제출할 결과물
- 목표 exploit 성공률

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
  - backing buffer는 광고된 길이로 `kmalloc()` 된다.
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
- 유효 정보 조건: 반복 heap grooming으로 함수 포인터 또는 direct-map 주소를
  포함한 동일 크기 객체를 재사용시켜 KASLR slide나 heap 주소를 계산할 수 있어야
  한다. 단순 nonzero tail만으로 Stage 1 성공을 확정하지 않는다.
- 우선 grooming 후보: userspace에서 생성/해제 가능한 `pipe_buffer` ring array.
  각 entry의 `struct page *`와 `anon_pipe_buf_ops`는 각각 direct-map 및 kernel
  text 기준 주소 후보이며 배열에 반복되어 유출 식별도 비교적 쉽다. 실제 Ubuntu
  24.04 config의 기본 ring 크기, kmalloc cache 및 구조체 offset은 pahole과 runtime
  관찰로 다시 확인해야 한다.

### Stage 2 — Custom driver

- 진입점: `custom_probe()`
- 매칭 조건 후보: vendor-specific interface class `0xff`, subclass `0x42`,
  protocol `0x01`
- 우선 검토 primitive: 장치가 제공하는 record count에 대해 pointer table 크기로
  allocation하지만 bulk endpoint에서는 실제 record 구조체 배열 크기를 수신하는
  under-allocation heap overflow. 정수 wrap만 노골적으로 삽입하는 것보다 실제
  driver의 metadata/payload 크기 혼동으로 보이게 한다.
- Stage 1 연결 후보: Stage 1으로 kernel text 또는 direct-map 기준 주소를 얻고,
  overflow 전에 live `pipe_buffer` ring array를 같은 cache에 grooming한다.
- 최종 공격 대상 후보: 인접 `pipe_buffer`의 `page`, `offset`, `len`을 변조해
  page-oriented read/write primitive로 발전시킨다. 함수 포인터를 직접 가짜
  userspace vtable로 바꾸는 방식은 SMEP 때문에 우선순위가 낮다.
- 상태: Hypothesis. allocation cache, target lifetime, 외부 trigger, SMEP/SMAP
  대응을 실제 kernel에서 검증하기 전에는 확정하지 않는다.

## 7. 검증할 가설

- [x] host model에서 short response의 미초기화 tail이 raw descriptor 출력으로
  전달된다.
- [x] QEMU Linux 6.8.12에서 kmalloc-1k `pipe_buffer` ring grooming 후
  canonical direct-map `struct page *` 후보가 5/5회 유출된다.
- [x] 해제된 pipe ring의 offset `0x200`에 유효한 `struct page *` 후보를
  반복 배치할 수 있다. 해제 과정에서 `ops`는 NULL이 되므로 같은 방법으로
  `anon_pipe_buf_ops`는 유출되지 않는다.
- [x] sysfs `descriptors`에서 광고 길이 1024바이트의 configuration tail을
  외부에서 읽을 수 있다.
- [x] QEMU `dummy_hcd`에서 Stage 1 이후 vendor interface가 생성된다.
- [x] QEMU Raw Gadget의 `ff/42/01` interface가 `kapo_ctf` driver에 매칭되어
  `custom_probe()`가 실행된다.
- [ ] Stage 1에서 얻은 정보나 상태를 Stage 2에서 이용할 수 있다.
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

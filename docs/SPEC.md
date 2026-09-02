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

### Stage 1 — USB core parser

- 후보 취약점: heap OOB read
- 후보 위치: `usb/core/config.c`
- 예상 입력: 조작된 configuration descriptor
- 현재 문제:
  - 대상 버퍼가 `kzalloc()`으로 초기화됨
  - 유용한 데이터가 실제로 유출되는지 확인 필요
  - 취약점 발생 후 enumeration이 계속되는지 확인 필요

### Stage 2 — Custom driver

- 진입점: `custom_probe()`
- 매칭 조건: TBD
- 후보 primitive: TBD
- Stage 1과 연결하는 방법: TBD
- 최종 공격 대상: TBD

## 7. 검증할 가설

- [ ] descriptor 길이 조작으로 OOB read가 발생한다.
- [ ] OOB 영역에 유용한 kernel pointer를 배치할 수 있다.
- [ ] 읽은 데이터를 외부에서 관찰할 수 있다.
- [ ] Stage 1 이후에도 USB enumeration이 완료된다.
- [ ] custom driver의 `probe()`가 실행된다.
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

# USB Kernel CTF

RP2040 Zero 또는 trusted Raw Gadget emulator가 제공하는 USB 입력으로 Linux 6.8
USB core의 Stage 1 heap disclosure와 CTF용 custom driver의 Stage 2 heap overflow를
연결하는 커널 CTF 프로젝트다.

## 현재 상태

- 현재 우선 개발 방향: QEMU-only software edition
- 후속 보존 방향: RP2040 hardware edition
- Target-equivalent Linux 6.8.12 QEMU full chain: PASS 10/10
- 비특권 UID 65534에서 UID 0 및 `KAPO{test_flag}` 획득: PASS 10/10
- 현재 kernel source rebuild 및 hardware initramfs boot smoke: PASS
- RP2040 firmware ARM build 및 host protocol self-test: PASS
- 로컬 hardware-validation bundle과 `SHA256SUMS`: BUILT
- 참가자용 pre-release 폴더, Ubuntu 24.04.4 initramfs와 실행 스크립트: BUILT
- 실제 RP2040 full chain: NOT TESTED

다음 이정표는 organizer-owned trusted USB emulator daemon을 QEMU guest에서 자동
시작하되 disconnected 상태로 대기시키고, 참가자가 UID 65534 guest exploit으로 모든
heap grooming과 trigger를 수행해 `KAPO{test_flag}`를 획득할 수 있는 방향 2 배포본을
만드는 것이다. 방향 1의 RP2040 firmware와 검증 자료는 후속 hardware edition을 위해
보존한다.

`dist/hardware-validation/`에 `bzImage`, `vmlinux`, `System.map`, kernel config,
test initramfs와 `kapo-rp2040.uf2`를 생성했다. 이 디렉터리는 Git에 포함하지 않는 로컬
검증 산출물이며 방향 1용으로 보존한다.

현재 `participant-release/`는 방향 1 RP2040 pre-release다. 방향 2 배포본이 완성될
때까지 참가자 최종 배포본으로 취급하지 않는다.

## 문서

- `docs/SPEC.md`: 확정된 문제 설계와 exploit chain
- `docs/HARDWARE-VALIDATION.md`: RP2040 flash 및 실제 장치 검증 절차
- `STATUS.md`: 이정표별 진행 상황, 검증 결과와 blocker
- `HANDOFF.md`: 현재 작업 트리, 검증 한계와 바로 다음 작업
- `AGENTS.md`: 저장소 작업 규칙과 세션 절차

실제 flag와 내부 검증 artifact는 저장소에 추가하지 않는다. `participant-release/`의
배포 바이너리는 이번 참가자 패키지를 위해 의도적으로 추적한다.

# USB Kernel CTF

RP2040 Zero 또는 trusted Raw Gadget emulator가 제공하는 USB 입력으로 Linux 6.8
USB core의 Stage 1 heap disclosure와 CTF용 custom driver의 Stage 2 heap overflow를
연결하는 커널 CTF 프로젝트다.

## 현재 상태

- 현재 우선 개발 방향: QEMU-only software edition
- 후속 보존 방향: RP2040 hardware edition
- QEMU-only 배포 후보 `testkernelctf/`와 `testkernelctf.tar.xz`: VALIDATED RC
- 현재 재현성 재빌드 artifact cold boot full chain 및 test flag: TCG PASS 5/5
- Target-equivalent Linux 6.8.12 QEMU full chain: PASS 10/10
- 비특권 UID 65534에서 UID 0 및 `KAPO{test_flag}` 획득: PASS 10/10
- 현재 kernel source rebuild 및 hardware initramfs boot smoke: PASS
- RP2040 firmware ARM build 및 host protocol self-test: PASS
- 로컬 hardware-validation bundle과 `SHA256SUMS`: BUILT
- 방향 1 참가자 pre-release 폴더와 RP2040 실행 파일: BUILT/PRESERVED
- 실제 RP2040 full chain: NOT TESTED

방향 2 배포 후보는 organizer-owned trusted USB emulator daemon을 QEMU guest에서 root로
시작하되 disconnected 상태로 대기시킨다. 참가자가 UID 65534 guest exploit으로 모든
heap grooming과 trigger를 수행한다. 현재 재현성 재빌드 후보에서 `KAPO{test_flag}` 획득을
TCG cold boot 5/5로 확인했다. 다음 이정표는 팀원의 LLM 기반 black-box 풀이 및 난이도
확인이다.

`dist/hardware-validation/`에 `bzImage`, `vmlinux`, `System.map`, kernel config,
test initramfs와 `kapo-rp2040.uf2`를 생성했다. 이 디렉터리는 Git에 포함하지 않는 로컬
검증 산출물이며 방향 1용으로 보존한다.

`participant-release/`는 방향 1 RP2040 pre-release로 보존한다. 방향 2 참가자에게는
`testkernelctf.tar.xz`만 전달한다.

## 문서

- `docs/SPEC.md`: 확정된 문제 설계와 exploit chain
- `docs/HARDWARE-VALIDATION.md`: RP2040 flash 및 실제 장치 검증 절차
- `STATUS.md`: 이정표별 진행 상황, 검증 결과와 blocker
- `HANDOFF.md`: 현재 작업 트리, 검증 한계와 바로 다음 작업
- `AGENTS.md`: 저장소 작업 규칙과 세션 절차

실제 flag와 내부 검증 exploit은 저장소에 추가하지 않는다. 방향 2 배포본에는
`KAPO{test_flag}`만 포함한다.

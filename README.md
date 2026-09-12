# YOASOBI

Trusted USB emulator가 제공하는 입력으로 Linux 6.8 USB core의 heap disclosure와
custom driver의 heap overflow를 연결하는 QEMU 커널 CTF 프로젝트다.

## 배포 구조

- `kernel/bzImage`: 서버와 참가자 배포본이 공통으로 사용하는 커널
- `server/initramfs.cpio.gz`: private 서버용 실제 flag image
- `participant/YOASOBI/`: 참가자 배포본 수정용 원본
- `participant/YOASOBI.tar.gz`: 참가자에게 전달할 archive

운영 배포본은 `v1r4m/poka2026`의 `poiri3r/13001/`에 있으며, 이 저장소에서 검증한
`kernel/`, `server/`와 참가자 패키지를 반영한다. `participant/`는 서버 실행 입력이
아니다.

## 현재 상태

- Linux 6.8.12 QEMU-only release candidate
- UID 65534에서 Stage 1 leak, Stage 2 overflow, UID 0 및 flag 획득: PASS
- 로컬 TCG와 Poka Docker 배포 endpoint full chain: PASS
- 운영 KVM: NOT TESTED

## 문서

- `docs/SPEC.md`: 문제 설계와 의도된 exploit chain
- `writeup/`: 출제자용 핵심 풀이와 최종 exploit
- `STATUS.md`: 진행 상황과 검증 결과

실제 flag와 최종 exploit은 private 저장소의 `server/`와 `writeup/`에만 두며
참가자 배포본에는 포함하지 않는다. 참가자 initramfs에는 `kapo{test_flag}`만 포함한다.

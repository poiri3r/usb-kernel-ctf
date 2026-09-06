# 프로젝트 진행 상태

이 문서는 이정표별 진행 상황과 검증 범위를 기록한다. 작업 재개 순서는
`docs/SPEC.md` → 이 문서 → `HANDOFF.md`이다.

최종 갱신: 2026-09-06 KST

## 현재 요약

| 범위 | 상태 | 근거와 한계 |
|---|---|---|
| 현재 개발 방향 | DIRECTION 2 | QEMU-only software edition을 먼저 완성하고 팀원 풀이 진행 |
| 방향 2 trusted USB emulator 및 배포본 | NOT IMPLEMENTED | 다음 세션의 현재 작업 |
| 문제 설계 및 QEMU exploit chain | PASS | Linux 6.8.12 target-equivalent cold boot 10/10 |
| 현재 kernel source | BUILD PASS | review fix와 hardware stage ioctl을 포함해 `bzImage` 재빌드 |
| hardware validation initramfs | BOOT PASS | 1 vCPU, 2 GiB QEMU에서 UID 65534 harness 시작 확인 |
| RP2040 firmware | BUILD PASS | Pico SDK 2.3.1 ARM build와 host protocol self-test 통과 |
| hardware-validation bundle | BUILT | 로컬 `dist/hardware-validation/`, artifact hash 검증 통과 |
| 참가자 pre-release | BUILD/BOOT PASS | `participant-release/`, Ubuntu 24.04.4, UID 65534 shell 확인 |
| 실제 RP2040 full chain | NOT TESTED | 보드 연결 후 확인 필요 |
| 참가자 최종 배포본 | NOT FINAL | 현재 폴더는 방향 1 RP2040 pre-release |

현재 단계는 **방향 2 QEMU-only 배포본 구현 시작 전**이다. RP2040 hardware edition은
방향 1로 보존하지만 당장 완료 조건으로 삼지 않는다.

## 2026-09-06 — QEMU-only 우선 개발 결정

- 방향 2를 먼저 완성해 팀원이 hardware 없이 participant archive만으로 풀도록 한다.
- 방향 1은 guest exploit과 RP2040 firmware를 함께 제출하는 hardware edition으로
  보존한다.
- 방향 2에서는 organizer-owned trusted USB emulator daemon만 root로 실행한다.
  daemon은 disconnected 상태로 기다리며 UID 65534 participant exploit의 readiness
  signal 뒤 USB를 연결한다. 참가자는 guest exploit만 제출한다.
- participant release에는 `KAPO{test_flag}`만 두고 실제 flag image는 운영진만
  보관한다.
- 참가자 exploit은 QEMU의 `/work`로 전달하며 host에서는 실행하지 않는다.
- 기존 Stage 1 leak, Stage 2 overflow와 page-cache overwrite chain은 변경하지 않는다.
- trusted emulator는 USB transport와 payload만 담당하며 helper/seq/pipe grooming은
  전부 participant exploit이 수행한다.
- 현재 `participant-release/`와 hardware bundle은 방향 1 산출물이므로 삭제하거나
  완료로 재표시하지 않는다.

## 2026-09-06 — 하드웨어 검증 파일 생성

- `firmware/`에 RP2040 USB controller 직접 제어 firmware와 build 절차를 추가했다.
- firmware는 helper leak, seq leak, Stage 2 세 모드를 제공하며 vendor request `0x5c`마다
  2초 disconnect 후 다음 모드로 reconnect한다.
- QEMU host process가 담당하던 helper/seq heap grooming을 guest hardware harness로
  옮겼다. firmware는 short descriptor, reconnect, transfer context, bulk payload만 맡는다.
- current `customdriver.c`는 stage transition ioctl, DMA-safe context 전송, per-device
  transaction mutex, unprivileged device-node mode를 포함한다.
- Linux 6.8.12 exact target source를 다시 빌드해 `bzImage`, `vmlinux`, `System.map`,
  kernel config를 생성했다.
- test initramfs와 RP2040 UF2를 묶어 `dist/hardware-validation/`을 만들고 모든 artifact의
  `SHA256SUMS`를 확인했다.
- QEMU boot smoke에서 current kernel의 `kapo_ctf` 등록, initramfs 실행,
  `HARDWARE_READY`, `EUID_BEFORE=65534`를 확인했다. 물리 USB 동작은 검증 범위 밖이다.
- `participant-release/`에 exact kernel, Ubuntu 24.04.4 initramfs, helper 분석 사본,
  QEMU/USB 연결 스크립트, 최소 RP2040 firmware template과 전체 hash를 모았다.
- 참가자 initramfs는 QEMU에서 Ubuntu 24.04.4, UID/GID 65534, 9p `/work` mount까지
  확인했다. 내부 full-chain harness와 완성 attack firmware는 포함하지 않는다.

## 이전 완료 이정표

- Stage 1 helper `struct page *`와 `proc_single_show` leak으로 runtime `page`, KASLR slide,
  `page_cache_pipe_buf_ops`를 계산한다.
- Stage 2의 2048-byte receive가 1024-byte allocation을 넘어 인접 live helper-backed
  `pipe_buffer`를 재구성한다.
- `PIPE_BUF_FLAG_CAN_MERGE` page-cache write로 root-owned mode `4755` helper를 바꾸고,
  비특권 UID 65534에서 UID 0과 `KAPO{test_flag}`를 획득했다.
- Linux 6.8.12, KASLR enabled, KASAN/SLUB redzone disabled, 1 vCPU, 2 GiB cold boot
  10/10회에서 full chain을 확인했다.
- Stage 1 host model, KASAN Stage 2 overflow, redzone-free adjacency, pipe metadata layout,
  page-cache write를 각각 검증했다.

## 남은 작업

1. 기존 Raw Gadget 코드에서 USB transport, descriptor와 payload 처리만 재사용해
   organizer-owned trusted USB emulator가 세 모드를 한 프로세스에서 제공하도록 한다.
   기존 gadget-side helper/seq/pipe grooming 코드는 재사용하지 않는다.
2. emulator를 root로 먼저 실행한 뒤 UID 65534 participant shell/exploit을 시작하는
   방향 2 initramfs를 만든다. daemon은 disconnected 상태로 기다리고 participant
   exploit이 grooming 뒤 readiness signal을 보내 첫 enumeration을 시작한다.
3. `/work`에는 participant exploit만 전달하고 emulator 교체나 `/dev/raw-gadget`
   직접 접근이 ticket 신뢰 경계를 우회하지 않는지 확인한다.
4. RP2040 firmware와 hardware 문서는 보존하되 방향 2 participant archive에서
   firmware, USB passthrough 의존성을 제거한다.
5. final symbol exposure 정책을 적용하고 archive에 internal material과 실제 flag가
   없는지 검사한다.
6. 방향 2 cold boot full chain 10/10 뒤 팀원에게 participant archive만 전달해
   black-box 풀이를 요청한다.
7. 팀원 결과를 반영한 뒤 방향 1 hardware edition 진행 여부를 결정한다.

single-enumeration dual configuration과 physical RP2040 검증은 방향 1의 후속 작업이며
현재 방향 2 완료 조건이 아니다.

## 검증 기록

### PASS

- 기존 target-equivalent QEMU full chain 10/10
- current Linux 6.8.12 `bzImage` rebuild
- current kernel + hardware initramfs boot smoke
- RP2040 firmware ARM build
- descriptor 및 Stage 2 payload host protocol self-test
- hardware harness `-Wall -Wextra -Werror` static build
- build/init shell script `sh -n`
- bundle `SHA256SUMS` 전체 검증
- 참가자 Ubuntu 24.04.4 initramfs boot, UID/GID 65534와 9p `/work` mount
- 참가자 firmware template ARM/UF2 build
- 참가자 폴더에 내부 exploit/harness가 없음을 archive listing으로 확인
- `git diff --check`, `git diff --cached --check`

### 미검증 또는 제한

- 방향 2 trusted USB emulator, initramfs와 participant archive는 NOT IMPLEMENTED다.
- 팀원 black-box 풀이는 NOT TESTED다.
- 실제 RP2040 USB enumeration 및 full chain은 NOT TESTED다.
- current kernel의 physical USB path와 three-reconnect runtime은 보드가 필요하다.
- current kernel source로 기존 Raw Gadget full chain 10/10을 다시 돌리지는 않았다.
- 참가자 QEMU 실행 명령은 작성했지만 KVM 및 physical USB passthrough는 미검증이다.
- 실제 flag 주입과 현장 ticket 운영 절차는 미확정이다.

## Git 상태

- Branch: `main`
- 이 문서를 포함하는 checkpoint에 full-chain, firmware, bundle build와 문서 변경을
  함께 기록한다.
- 생성된 `dist/`와 `firmware/build/`은 `.gitignore` 대상이다.
- `participant-release/`의 pre-release 배포 파일과 바이너리는 이번 요청에 따라 추적한다.

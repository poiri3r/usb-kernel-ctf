# 프로젝트 진행 상태

이 문서는 이정표별 진행 상황과 검증 범위를 기록한다. 작업 재개 순서는
`docs/SPEC.md` → 이 문서 → `HANDOFF.md`이다.

최종 갱신: 2026-09-06 KST

## 현재 요약

| 범위 | 상태 | 근거와 한계 |
|---|---|---|
| 문제 설계 및 QEMU exploit chain | PASS | Linux 6.8.12 target-equivalent cold boot 10/10 |
| 현재 kernel source | BUILD PASS | review fix와 hardware stage ioctl을 포함해 `bzImage` 재빌드 |
| hardware validation initramfs | BOOT PASS | 1 vCPU, 2 GiB QEMU에서 UID 65534 harness 시작 확인 |
| RP2040 firmware | BUILD PASS | Pico SDK 2.3.1 ARM build와 host protocol self-test 통과 |
| hardware-validation bundle | BUILT | 로컬 `dist/hardware-validation/`, artifact hash 검증 통과 |
| 참가자 pre-release | BUILD/BOOT PASS | `participant-release/`, Ubuntu 24.04.4, UID 65534 shell 확인 |
| 실제 RP2040 full chain | NOT TESTED | 보드 연결 후 확인 필요 |
| 참가자 최종 배포본 | NOT FINAL | 실제 장치와 USB passthrough 결과 미확정 |

현재 단계는 **검증용 펌웨어·커널 파일 생성 완료, 실제 RP2040 검증 대기**다.

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

1. `docs/HARDWARE-VALIDATION.md`에 따라 UF2를 실제 RP2040 Zero에 flash한다.
2. supplied kernel/initramfs에 보드를 passthrough하고 enumeration, 두 leak, Stage 2,
   UID 0, `KAPO{test_flag}`를 확인한다.
3. 첫 성공 뒤 동일 조건에서 반복 실행해 안정성을 측정하고 USB passthrough 명령을
   문서에 고정한다.
4. 실제 장치 결과를 반영한 뒤 USB passthrough 절차와 실제 flag 주입 절차를 고정하고
   `participant-release/README.md`의 pre-release 표시를 제거한다.

single-enumeration dual configuration은 선택적 최적화이며 현재 three-reconnect 방식의
완료 조건이 아니다.

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
- 커밋 전 독립 Codex read-only review에서 신규 actionable finding 0건을 확인했다.

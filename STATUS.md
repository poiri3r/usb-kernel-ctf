# 프로젝트 진행 상태

이 문서는 이정표별 진행 상황과 검증 범위를 기록한다. 작업 재개 순서는
`docs/SPEC.md` → 이 문서 → `HANDOFF.md`이다.

최종 갱신: 2026-09-07 KST

## 현재 요약

| 범위 | 상태 | 근거와 한계 |
|---|---|---|
| 현재 개발 방향 | DIRECTION 2 | QEMU-only software edition을 먼저 완성하고 팀원 풀이 진행 |
| 방향 2 trusted USB emulator 및 배포본 | RUNTIME PASS | 재현성 재빌드 artifact TCG cold boot 5/5 |
| 문제 설계 및 QEMU exploit chain | PASS | Linux 6.8.12 target-equivalent 기존 10/10, 현재 후보 5/5 |
| 현재 kernel source | BUILD PASS | old-ring 초기화, exact pipe ops 검증, devnode fix 포함 |
| hardware validation initramfs | BOOT PASS | 1 vCPU, 2 GiB QEMU에서 UID 65534 harness 시작 확인 |
| RP2040 firmware | BUILD PASS | Pico SDK 2.3.1 ARM build와 host protocol self-test 통과 |
| hardware-validation bundle | BUILT | 로컬 `dist/hardware-validation/`, artifact hash 검증 통과 |
| 참가자 pre-release | BUILD/BOOT PASS | `participant-release/`, Ubuntu 24.04.4, UID 65534 shell 확인 |
| 실제 RP2040 full chain | NOT TESTED | 보드 연결 후 확인 필요 |
| 참가자 배포 후보 | HARDENED RC | resize shortcut 차단 artifact, 의도 체인 TCG 5/5 |
| 힌트 축소 독립 AI 풀이 | PASS 3/3 | 658초, 823초, 1,026초; median 823초 |
| 알려진 resize shortcut | BLOCKED 3/3 | 기존 A/B/C exploit 모두 helper leak 단계 실패 |
| 최종 강화본 fresh AI 풀이 | PASS | 1,190초, 의도 체인, shortcut 없음, 8/10 |

현재 단계는 **강화 배포본의 추가 표본 수집과 최종 독립 검토**다.
RP2040 hardware edition은 방향 1로 보존하지만 현재 완료 조건은 아니다.

## 2026-09-07 — helper-stage resize shortcut 차단

- 원인: `pipe_resize_ring()`이 live `pipe_buffer`를 새 ring에 복사한 뒤 old ring을 그대로
  해제하여 helper `struct page *`와 runtime `page_cache_pipe_buf_ops`가 kmalloc-1k stale
  data로 함께 남았다.
- `kernel-patches/0001-pipe-clear-resized-ring.patch`는 old ring 해제를
  `kfree_sensitive()`로 바꾼다. `scripts/build-target-kernel.sh`가 exact Linux 6.8.12
  source에 이 overlay를 idempotent하게 적용한다.
- 정상 pipe close의 entry release와 `ops = NULL` 경로는 변경하지 않아 helper page-only
  leak을 유지한다.
- 강화 kernel build: PASS. 최종 `bzImage` SHA-256은
  `6dea02044767b603a715d9aba6a3b520b1d374d31e2de3a0785d2044de842eb8`이다.
- 기존 독립 풀이 A/B/C의 resize shortcut binary는 강화 artifact에서 3/3 helper leak에
  실패했고 test flag를 얻지 못했다.
- Stage 2 ioctl은 context ops가 runtime `page_cache_pipe_buf_ops`와 정확히 일치할 때만
  진행한다. 0x40 낮은 대체 pipe ops를 넣은 회귀 실행은 helper/seq leak 뒤 `EINVAL`로
  거부됐다.
- organizer harness는 seq readiness FIFO를 grooming 전에 열고 512개 seq object 중
  마지막 128개를 해제한다. QEMU emulator는 32-byte seq configuration을 8개 제공하며
  hardware mode는 1개를 유지한다. 최종 artifact cold boot 5/5에서 helper page leak,
  seq KASLR leak, page-cache overwrite, UID 0, `KAPO{test_flag}`, `FULL_CHAIN_PASS`를
  모두 확인했다.
- 최종 강화 `testkernelctf.tar.xz` SHA-256은
  `a5e93a01ff1a4af9d8570cdb9ec660e098d992eced7f0781d83690e055418a66`이다.
- 동일 입력으로 initramfs와 archive를 각각 다시 빌드해 byte-for-byte 일치를 확인했다.
- 이전 독립 exploit을 최종 artifact에 맞춘 회귀 실행은 163초에 성공했다. 이 실행은
  seq leak을 실제 사용하고 runtime `page_cache_pipe_buf_ops`만 전달했으며, 이미 완성된
  exploit의 적응 시간이므로 신규 black-box 난이도 표본에는 포함하지 않는다.
- 최종 archive만 받은 fresh 세션 C는 1,190초(19분 50초)에 UID 65534에서
  `KAPO{test_flag}`를 획득했다. 정상 close 뒤 stale ops가 0임을 확인하고 helper page-only
  leak, seq KASLR leak, exact runtime `page_cache_pipe_buf_ops`, Stage 2 overflow와 SUID
  helper page-cache overwrite를 순서대로 사용했다. shortcut은 보고하지 않았고 난이도는
  8/10으로 평가했다.
- 독립 소스 검토는 신규 쟁점 0이다. pipe ring의 참가자 접근 가능 free 경로에서 resize는
  old ring 전체를 초기화하고 정상 close는 entry별 `ops = NULL` 뒤 해제함을 대조했다.
  별도 FUSE pipe-buffer 경로는 target config에서 비활성화되어 있다. 강화 archive 신규
  black-box 풀이는 진행 중이다.

## 2026-09-07 — 힌트 축소 후 독립 AI black-box 풀이

- participant README에서 second-stage heap 준비와 final bind 이후 live pipe layout timing
  문구를 삭제하고 readiness signal 이름과 순서만 남겼다.
- 변경 artifact의 organizer full chain smoke는 UID 65534, UID 0,
  `KAPO{test_flag}`, `FULL_CHAIN_PASS`: PASS.
- 변경 archive SHA-256은
  `b477481489e6c7284c0c211de2ef6b5506ff422907c7963ffa79b39efd61d26e`이다.

| 세션 | 결과 | 시간 | 자체 난이도 | 비고 |
|---|---|---:|---:|---|
| A | PASS | 1,026초 | 8/10 | clean VM 2회 성공, seq KASLR leak 우회 |
| B | PASS | 823초 | 7/10 | fresh boot 재현 PASS, seq KASLR leak 우회 |
| C | PASS | 658초 | 7/10 | blocker 없음, seq KASLR leak 우회 |

- 3개 독립 세션 평균은 836초(13분 56초), 중앙값은 823초(13분 43초), 범위는
  658~1,026초다. 세션은 변경 archive만 사용했고 내부 저장소, 기존 exploit과 검증
  로그를 읽지 않았다.
- Confirmed shortcut: 세 세션 모두 helper stage에서 resize로 해제한 live pipe ring으로
  helper `struct page *`와 runtime `page_cache_pipe_buf_ops`를 동시에 leak했다. 따라서
  seq enumeration은 통과했지만 seq pointer/KASLR leak은 사용하지 않았다.
- 이 표본은 힌트 축소 archive에서도 10~20분 내 풀이가 가능했고, 세 풀이 모두 같은
  resize shortcut을 사용했음을 보여준다. 강화 archive의 난이도는 재측정 중이다.

## 2026-09-07 — Jeopardy 서버 운영 방식 확정

- 수동 ticket 검증 대신 challenge TCP 연결마다 disposable QEMU serial console을 제공한다.
- 참가자는 로컬 배포본에서 exploit을 개발하고 TCP 연결 직후 base64 ELF로 업로드한 뒤
  원격 UID 65534 shell에서 실행한다.
- 실제 flag가 든 initramfs는 서버에만 보관하며 QEMU monitor와 host filesystem은
  참가자에게 노출하지 않는다.
- `scripts/jeopardy_qemu_server.py`에 표준 라이브러리 기반 TCP/QEMU launcher를 구현했다.
  동시 실행 상한, session timeout, 연결 종료 시 process-group cleanup을 포함한다.
- launcher가 생성하는 QEMU에는 session별 exploit directory만 `/work`로 연결되며,
  monitor와 guest network는 없다. server 종료 시에도 실행 중 QEMU를 정리한다.
- 외부 flag 파일을 다섯 번째 인자로 받는 server-only initramfs build 경계와 systemd
  격리 unit을 추가했다. builder는 종료 시 flag가 든 임시 rootfs와 cpio를 삭제한다.
  실제 운영 flag는 저장소나 배포 archive에 포함하지 않는다.
- localhost TCP shell에서 public test initramfs와 별도 server test flag initramfs를
  각각 부팅했다. 두 경우 모두 UID 65534 full chain, UID 0과 flag 획득: PASS.
- 연결 종료와 launcher `SIGTERM` 뒤 QEMU process 및 session directory cleanup: PASS.

## 2026-09-07 — QEMU-only 배포 후보

- `scripts/trusted-usb-emulator.c`에 세 단계 Raw Gadget emulator를 구현했다. root
  daemon은 USB transport, descriptor, reconnect, transfer context 역직렬화와 그 값에
  따른 Stage 2 payload만 담당하며 helper/seq/pipe grooming이나 flag 접근은 하지 않는다.
- `scripts/participant-init.sh`가 daemon을 root로 시작하고 readiness FIFO를 만든 뒤
  UID 65534 shell을 연다. 참가자 exploit은 helper/seq grooming과 Stage 2 pipe 준비를
  모두 직접 수행한다.
- 첫 방향 2 실행에서 disconnect cleanup의 `custom_devnode()` NULL dereference를
  재현했고 `mode`가 존재할 때만 권한을 기록하도록 수정했다.
- Linux 6.8.12 배포 커널을 재빌드하고 `CONFIG_KALLSYMS_ALL`을 비활성화했다.
  `System.map`은 제외했으며 배포 `vmlinux`와 helper는 `strip --strip-all`을 적용했다.
- `testkernelctf/`에는 QEMU-only 실행에 필요한 파일만 남겼고 RP2040 firmware,
  USB passthrough script, organizer exploit/harness와 내부 문서를 제외했다.
- 고정 build identity와 epoch를 적용해 kernel/rootfs/archive를 다시 만들었다.
- 재현성 재빌드한 정확한 artifact로 QEMU 8.2.2, 1 vCPU, 2 GiB, TCG cold boot
  full chain을 5/5회 실행했다. 매 회 UID 65534에서 helper-page leak, KASLR leak,
  page-cache overwrite, UID 0, `KAPO{test_flag}`, `FULL_CHAIN_PASS`를 확인했다.
- 최초 `testkernelctf.tar.xz` archive SHA-256은
  `b161e85ed2bba672d4af68c919e838b9b950f2ea94a90997b5082b57bbaeedc8`이었다.
  현재 힌트 축소 archive hash는 위 black-box 풀이 절을 따른다.

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

1. 진행 중인 추가 fresh 세션 B의 성공 또는 30분 censored 결과를 기록한다.
2. 최종 diff/source 검토에서 신규 쟁점 0을 확인한다.
3. 운영 서버에서 실제 flag 파일로 initramfs를 빌드하고 systemd unit을 설치한다.
4. 방향 1 hardware edition 진행 여부를 이후 결정한다.

single-enumeration dual configuration과 physical RP2040 검증은 방향 1의 후속 작업이며
현재 방향 2 완료 조건이 아니다.

## 검증 기록

### PASS

- Jeopardy localhost TCP public/server-test-flag full chain
- 힌트 축소 archive organizer smoke full chain
- 변경 archive 독립 AI black-box 풀이 3/3
- 강화 kernel build와 archive 내부 `SHA256SUMS`
- 강화 artifact 의도 full chain TCG cold boot 5/5
- 기존 resize shortcut exploit 차단 3/3
- 독립 source/diff 검토 신규 쟁점 0
- 최종 initramfs/archive 재현성 rebuild byte-for-byte 일치
- 최종 제약에 맞춘 독립 exploit 회귀 flag 획득
- 최종 archive fresh participant-only 의도 풀이 flag 획득 (1,190초)
- client disconnect 및 launcher `SIGTERM` QEMU/session cleanup
- launcher Python compile과 unit test 3개
- server-only initramfs test flag 주입 및 `/flag` mode `0400`
- server-only initramfs build 이후 flag 포함 임시 파일 cleanup
- systemd unit 정적 검증과 offline security score `4.3 OK`
- 방향 2 trusted emulator 정적 빌드 (`-Wall -Wextra -Werror`, static, stripped)
- 방향 2 Linux 6.8.12 kernel rebuild
- 재현성 재빌드 QEMU-only 후보 full chain TCG cold boot 5/5
  (UID 65534 → UID 0 → `KAPO{test_flag}`)
- 고정 identity의 Linux 6.8.12 kernel rebuild
- 동일 입력 initramfs 2회 byte-for-byte 일치
- 배포 `vmlinux`와 helper의 symbol table 제거 확인 (`nm`: no symbols)
- `testkernelctf/SHA256SUMS` 전체 검증
- archive listing에서 organizer exploit/harness, 내부 문서, RP2040 파일 제외 확인
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
- 현재 후보의 KVM 경로는 NOT TESTED다.
- 실제 운영 flag는 사용하지 않았으며 외부 test flag 주입 경로만 PASS다.
- 운영 서버의 systemd runtime과 KVM 경로는 NOT TESTED다.

## Git 상태

- Branch: `main`
- 이 문서를 포함하는 checkpoint에 full-chain, firmware, bundle build와 문서 변경을
  함께 기록한다.
- 생성된 `dist/`와 `firmware/build/`은 `.gitignore` 대상이다.
- `participant-release/`는 방향 1 pre-release로 보존한다.
- `testkernelctf/`와 `testkernelctf.tar.xz`가 방향 2 배포 후보 산출물이다.

## 실측 난이도 평가

- AI 허용 환경 판정: **8/10, 상**. 최종 강화본 fresh 성공 표본은 19분 50초였고,
  이전 10분 58초~17분 06초 표본의 공통 shortcut은 현재 차단됐다.
- 사람 중심 판정: **상**. Stage 1 USB core leak과 Stage 2 custom driver overflow를 연결하고,
  KASLR 및 vmemmap 계산, pipe heap grooming, page-cache overwrite까지 참가자 exploit이
  직접 구현해야 한다.
- 배포 `vmlinux`와 helper는 stripped 상태이고 `System.map`이 없어 정적 분석 부담이 있다.
- emulator contract와 세 단계 readiness 순서는 공개되므로 USB transport 구현 부담은
  줄지만 kernel heap exploit 난이도는 유지된다.
- 최종 강화본에서 동급 AI의 현재 실측 성공 시간은 약 20분이다. 표본 수가 1개라 분산과
  상한은 아직 확정하지 않는다.

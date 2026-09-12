# 프로젝트 진행 상태

이 문서는 QEMU-only YOASOBI 문제의 현재 상태와 검증 결과를 기록한다.
작업 재개 순서는 `docs/SPEC.md` → 이 문서 → `README.md`이다.

최종 갱신: 2026-09-12 KST

## 현재 요약

| 범위 | 상태 | 근거와 한계 |
|---|---|---|
| 문제 설계와 exploit chain | PASS | Stage 1 leak, Stage 2 16-byte overflow, page-cache overwrite |
| 참가자 배포본 | PASS | `participant/YOASOBI.tar.gz`, test flag만 포함 |
| private 서버 image | PASS | 실제 flag full chain 확인 |
| 원격 Jeopardy endpoint | PASS | Poka PR #13 배포 후 `13.125.248.224:13001` 재검증 |
| KVM | NOT TESTED | 운영 배포는 TCG 사용 |

## 2026-09-12 — QEMU-only 저장소 정리

- RP2040 전용 `firmware/`, `participant-release/`, hardware validation 문서·산출물·빌드
  스크립트를 제거했다.
- `helpers/kapo-helper.c`는 현재 initramfs 빌드에 필요한 코드이므로
  `scripts/kapo.c`로 이동하고 builder 참조를 갱신했다.
- 기존 인계 문서와 session helper를 제거하고 진행 상황을 `STATUS.md`로 일원화했다.
- 이전 배포 확인용 `배포테스트1/`과 ignored `dist/hardware-validation/`은 휴지통으로
  이동했다.

## 2026-09-12 — readiness 규약 단순화

- readiness FIFO를 `/tmp/yoasobi-ready`로 바꾸고 세 번 모두 `ready` 한 줄만 받도록
  emulator, init, 공식 exploit과 참가자 문서를 맞췄다.
- 이름 변경 뒤 남아 있던 이전 참가자 경로 참조를 `participant/YOASOBI/`로
  바로잡았다.
- public/private initramfs와 `participant/YOASOBI.tar.gz`를 다시 생성했다.
- 수정된 공식 exploit로 public/private TCG full chain을 다시 확인했다: PASS.
- 참가자 README에서 Stage 수, enumeration과 next-stage ioctl 순서 힌트를 제거했다.
- Poka PR #13 merge와 deploy workflow 성공 후 현재 endpoint full chain을 확인했다: PASS.

## 2026-09-12 — 최종 저장소 축소

- 사용하지 않는 전체 upstream USB source 대신 kernel builder가 참조하는 overlay 5개와
  설명 문서만 `usb/`에 남겼다.
- 실제 배포와 무관한 기존 운영 unit과 문서를 제거하고 Poka Docker 배포를 기준으로
  문서를 갱신했다.
- Poka에 중복된 local launcher와 전용 test, Git session helper를 제거해 `scripts/`에는
  재빌드에 필요한 6개 파일과 사용법 문서만 남겼다.

## 현재 산출물

- `kernel/bzImage`: 서버와 참가자 공통 커널
- `server/initramfs.cpio.gz`: private 실제 flag image
- `participant/YOASOBI/`: 참가자 패키지 수정용 원본
- `participant/YOASOBI.tar.gz`: 참가자 전달 archive
- `writeup/`: 공식 풀이, source, payload, 최종 static ELF

## 검증 기록

PASS:

- 최종 `writeup/exploit`: static x86-64 ELF, warnings-as-errors build
- participant TCG: UID 65534 → page-cache write → UID 0 → `kapo{test_flag}`
- private TCG: UID 65534 → UID 0 → 실제 flag → `FULL_CHAIN_PASS`
- Poka PR #13 배포 후 원격 `13.125.248.224:13001`: `PAGE_CACHE_WRITE_PASS`, UID 0,
  실제 flag, `FULL_CHAIN_PASS`
- participant archive에 write-up, exploit, 실제 flag가 없음을 확인

NOT TESTED:

- KVM 실행 경로
- 팀원이 보고한 cross-cache `struct cred` overwrite 대체 풀이의 현재 산출물 재현

## 다음 작업

1. 보고된 cross-cache `struct cred` overwrite 풀이를 허용할지 완화할지 결정한다.

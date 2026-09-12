# YOASOBI Project Instructions

## Project goal

이 저장소의 목표는 trusted USB emulator가 전달하는 USB 입력으로 Linux 6.8의
USB 처리 경로와 CTF용 custom driver를 연결해 재현 가능한 QEMU 커널 CTF 문제를
제작하는 것이다.

설계의 일관성, 재현성, 의도된 exploit chain의 보존을 우선한다.

## Required context

작업을 시작하기 전에 다음 순서로 현재 상태를 확인한다.

1. `docs/SPEC.md`: 문제 설계와 의도된 exploit chain
2. `STATUS.md`: 전체 진행 상황, 현재 작업, blocker
3. `README.md`: 프로젝트의 간단한 개요
4. `git status --short --branch`와 최근 Git 기록

문서와 구현이 충돌하면 임의로 한쪽을 기준으로 작업하지 않는다. 충돌 내용을
보고하고, 사용자 결정이나 재현 가능한 실험 결과를 통해 기준을 갱신한다.

## Source of truth

- 확정된 문제 설계의 기준은 `docs/SPEC.md`이다.
- 현재 진행 상황의 기준은 `STATUS.md`이다.
- Git 커밋과 테스트 출력은 완료 여부를 뒷받침하는 근거다.
- 채팅 기억이나 Codex 로컬 메모리만을 프로젝트 사실의 근거로 사용하지 않는다.

`Confirmed`, `Hypothesis`, `Unverified`, `Blocked`, `TBD`를 구분한다. 실제로
재현하지 않은 내용을 `Confirmed` 또는 완료로 표시하지 않는다.

## Design discipline

- `docs/SPEC.md`의 확정된 취약점 위치, 공격자 모델, Stage 순서와 최종 목표를
  임의로 변경하지 않는다.
- 검토 중인 primitive는 가설로 취급하며, 실험 결과 없이 확정하지 않는다.
- primitive를 검증할 때 입력, 실행 환경, 명령, 관찰 결과와 실패 조건을 기록한다.
- Stage 1 변경은 USB enumeration과 custom driver의 `probe()` 도달 여부를 함께
  검증한다.
- Stage 2 변경은 Stage 1 결과와의 데이터 흐름 및 외부 관찰 가능성을 설명한다.
- 의도된 취약점을 실수로 제거하거나, 관련 없는 취약점을 추가하지 않는다.

## Repository boundaries

- `usb/`의 실제 코드에는 Linux 6.8.12 source에 덮어쓸 최소 overlay만 둔다:
  `core/config.c`, `core/message.c`, `class/customdriver.c`, `class/Kconfig`,
  `class/Makefile`.
- 전체 upstream USB source를 저장소에 다시 복사하지 않는다.
- 운영 배포는 `v1r4m/poka2026`의 `poiri3r/13001/`을 사용한다. 이 저장소의 검증된
  kernel, private initramfs와 participant package만 동기화한다.
- 경로나 구조를 변경해야 하면 먼저 `docs/SPEC.md`와 현재 구현의 차이를 보고한다.
- 참가자 archive와 private 저장소의 `server/initramfs.cpio.gz`는 Git에 둘 수 있다.
  원문 flag 파일, 그 밖의 빌드 산출물, 토큰, 키와 자격 증명은 Git에 추가하지 않는다.

## Shared progress protocol

- `STATUS.md`의 제목, 설명, 진행 상태와 검증 결과는 한국어로 작성한다. 코드 식별자,
  명령, 로그 문자열과 `PASS`, `FAIL`, `NOT TESTED` 같은 판정 표시는 원문을 유지할
  수 있다.

의미 있는 작업을 마칠 때 `STATUS.md`를 다음 기준으로 갱신한다.

- 완료된 작업
- 진행 중인 작업과 담당 범위
- 다음 작업
- blocker와 미결정 사항
- 실행한 테스트와 `PASS`, `FAIL`, `NOT TESTED` 결과
- 결과가 대응되는 브랜치와 커밋(알 수 있는 경우)

여러 에이전트가 병렬로 작업하는 경우 각 에이전트는 담당 파일과 범위를 먼저
명시하고 다른 에이전트의 진행 중인 파일을 덮어쓰지 않는다.

## Git workflow

- 새 커밋 메시지는 한국어로 작성한다. 코드 식별자나 subsystem 이름은 필요한 경우
  원문을 유지할 수 있다.

- 작업 전 현재 브랜치, upstream 상태, staged/unstaged/untracked 파일을 확인한다.
- 기존 미커밋 변경은 사용자 또는 다른 에이전트의 작업으로 간주해 보존한다.
- 작업 트리가 더러운 상태에서 자동으로 pull, rebase 또는 branch 전환을 하지 않는다.
- `git reset --hard`, 강제 push, 기존 변경 삭제를 사용하지 않는다.
- 병렬 작업은 가능하면 작업별 브랜치나 worktree를 사용한다.
- 커밋은 한 가지 목적에 집중하고, 관련 코드와 상태 문서 갱신을 함께 포함한다.
- 성공적으로 완료되고 검증된 작업은 사용자가 요청한 경우에만 commit하고 push한다.
  미완성 작업, 실패한 테스트 또는 범위가 불명확한 변경은 commit하지 않는다.
- merge와 pull request 생성은 사용자의 명시적 요청 없이 수행하지 않는다.
- 다른 기기에서 작업을 시작할 때 최신 원격 상태와 로컬 변경의 차이를 먼저 확인한다.

### Bounded finish and review

- 사용자가 현재 작업의 commit과 push를 이미 요청했다면 완료 시 다시 확인하지 않고
  검증 직후 관련 파일만 commit하고 push한다.
- 독립 검토에는 현재 diff, 상태 문서와 변경 주장을 판정하는 데 필요한 구현만 전달한다.
  저장소 전체 재탐색, 스킬 재독, 전체 빌드는 검토 근거에 필요할 때만 수행한다.
- 자동 독립 검토는 작업 단위당 최대 2회다. 첫 검토의 발견을 수정하면 한 번만
  재검토한다. 재검토에서 새 쟁점이 나오면 자동 수정·재검토 루프를 중단하고 사용자에게
  쟁점과 현재 diff를 보고한다.
- 문서 전용 변경은 `git diff --check`와 문서-구현 대조만 수행한다. 코드·artifact가
  바뀌지 않았다면 kernel build와 QEMU test를 반복하지 않는다.
- 검토 통과 뒤 review 결과를 기록하려고 상태 문서를 다시 수정하지 않는다. 검토 결과는
  commit/push 완료 보고에 남겨 검토 후 diff 변경을 피한다.
- 종료 보고는 변경, 실행한 검증, 미검증 항목, 다음 작업만 간단히 적는다.

## Desktop-notebook switching protocol

- 동시에 두 기기에서 작업하지 않는다. 한 기기의 변경을 commit하고 push한 뒤 다른
  기기에서 이어서 작업한다.
- 세션을 시작할 때 `git status --short --branch`를 실행한다. 작업 트리가 깨끗할
  때만 `git pull --ff-only`를 실행하고, 이후 `STATUS.md`를 읽는다.
- 세션을 마칠 때 코드와 함께 `STATUS.md`를 갱신한다.
- `git diff --check`와 필요한 테스트를 실행한 뒤 관련 파일만 stage한다. commit과
  push는 사용자가 변경 내용을 확인한 후 수행한다.
- 백그라운드 자동 pull, 자동 commit, 자동 push는 사용하지 않는다. 동기화 스크립트는
  작업 트리가 더럽거나 fast-forward가 불가능하면 변경 없이 중단해야 한다.
- push가 완료되지 않은 기기에서는 작업이 인계된 것으로 표시하지 않는다. 다른 기기는
  원격 commit을 확인한 뒤 작업을 시작한다.

## Build and verification

- 정확한 Linux 버전과 kernel config를 확인하지 않은 상태에서 호환성을 단정하지 않는다.
- 실행하지 않은 빌드나 테스트를 성공했다고 표현하지 않는다.
- 실패한 명령도 원인 분석에 필요하면 `STATUS.md`에 기록한다.
- 가능하면 최소한 다음 경로를 검증한다.

  1. 대상 커널 또는 모듈 빌드
  2. trusted emulator USB enumeration
  3. Stage 1 trigger
  4. enumeration 이후 custom driver matching과 `probe()` 실행
  5. Stage 2 primitive
  6. 반복 실행 안정성
  7. test flag 획득

환경이 없어 검증하지 못한 항목은 `NOT TESTED`로 표시한다.

## CTF and secret handling

- 작업 범위는 이 CTF 저장소와 명시적으로 허가된 로컬/VM 환경으로 제한한다.
- 실제 제3자 시스템이나 허가되지 않은 장치를 대상으로 테스트하지 않는다.
- 실제 운영 flag와 공식 exploit은 private 저장소의 `server/initramfs.cpio.gz`와
  `writeup/`에만 둘 수 있다.
- 예시 값은 `kapo{test_flag}`처럼 명백한 테스트 값만 사용한다.
- `docs/SPEC.md`, 공식 solve, exploit과 내부 검증 자료는 참가자 배포본에 포함하지 않는다.

## Completion report

작업을 마칠 때 다음을 간단히 보고한다.

1. 변경한 파일과 변경 목적
2. 실행한 테스트와 결과
3. 검증하지 못한 항목
4. 설계에 영향을 주는 결정 또는 새 가설
5. 다음 작업자가 이어서 할 일

# USB Kernel CTF Project Instructions

## Project goal

이 저장소의 목표는 RP2040 Zero가 전달하는 USB 입력으로 Linux 6.8의
USB 처리 경로와 CTF용 custom driver를 연결해 재현 가능한 커널 CTF 문제를
제작하는 것이다.

설계의 일관성, 재현성, 의도된 exploit chain의 보존을 우선한다.

## Required context

작업을 시작하기 전에 다음 순서로 현재 상태를 확인한다.

1. `docs/SPEC.md`: 문제 설계와 의도된 exploit chain
2. `STATUS.md`: 전체 진행 상황, 현재 작업, blocker
3. `HANDOFF.md`: 이전 작업의 변경 파일, 검증 결과, 다음 작업
4. `README.md`: 프로젝트의 간단한 개요
5. `git status --short --branch`와 최근 Git 기록

문서와 구현이 충돌하면 임의로 한쪽을 기준으로 작업하지 않는다. 충돌 내용을
보고하고, 사용자 결정이나 재현 가능한 실험 결과를 통해 기준을 갱신한다.

## Source of truth

- 확정된 문제 설계의 기준은 `docs/SPEC.md`이다.
- 현재 진행 상황의 기준은 `STATUS.md`이다.
- 컴퓨터, 노트북 또는 다른 에이전트로 작업을 넘길 때는 `HANDOFF.md`를 사용한다.
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

- `usb/`는 Linux 6.8 USB 드라이버 소스를 기준으로 가져온 영역이다.
- CTF 설계상 지정된 파일 외의 upstream 코드는 불필요하게 수정하지 않는다.
- USB core의 의도적 변경은 현재 `usb/core/config.c`를 대상으로 한다.
- CTF용 custom driver는 현재 `usb/class/customdriver.c`를 대상으로 한다.
- 경로나 구조를 변경해야 하면 먼저 `docs/SPEC.md`와 현재 구현의 차이를 보고한다.
- 빌드 산출물, VM 이미지, 실제 flag, 토큰, 키와 자격 증명을 Git에 추가하지 않는다.

## Shared progress protocol

의미 있는 작업을 마칠 때 `STATUS.md`를 다음 기준으로 갱신한다.

- 완료된 작업
- 진행 중인 작업과 담당 범위
- 다음 작업
- blocker와 미결정 사항
- 실행한 테스트와 `PASS`, `FAIL`, `NOT TESTED` 결과
- 결과가 대응되는 브랜치와 커밋(알 수 있는 경우)

작업을 다른 기기나 에이전트가 이어받아야 하면 `HANDOFF.md`에 다음을 기록한다.

- 작업 목표와 현재 브랜치
- 기준 커밋과 현재 커밋
- 변경한 파일
- 실행한 명령과 테스트 결과
- 확인된 사실과 아직 검증되지 않은 가설
- 남은 문제와 다음에 실행할 정확한 작업

여러 에이전트가 병렬로 작업하는 경우 각 에이전트는 담당 파일과 범위를 먼저
명시하고, 다른 에이전트의 진행 중인 파일을 덮어쓰지 않는다. 하나의
`HANDOFF.md`에서 충돌이 예상되면 `docs/handoffs/<task-name>.md`를 사용한다.

## Git workflow

- 작업 전 현재 브랜치, upstream 상태, staged/unstaged/untracked 파일을 확인한다.
- 기존 미커밋 변경은 사용자 또는 다른 에이전트의 작업으로 간주해 보존한다.
- 작업 트리가 더러운 상태에서 자동으로 pull, rebase 또는 branch 전환을 하지 않는다.
- `git reset --hard`, 강제 push, 기존 변경 삭제를 사용하지 않는다.
- 병렬 작업은 가능하면 작업별 브랜치나 worktree를 사용한다.
- 커밋은 한 가지 목적에 집중하고, 관련 코드와 상태 문서 갱신을 함께 포함한다.
- 성공적으로 완료되고 검증된 작업은 아래 session finish 절차로 commit하고 push한다.
  미완성 작업, 실패한 테스트 또는 범위가 불명확한 변경은 자동으로 commit하지 않는다.
- merge와 pull request 생성은 사용자의 명시적 요청 없이 수행하지 않는다.
- 다른 기기에서 작업을 시작할 때 최신 원격 상태와 로컬 변경의 차이를 먼저 확인한다.

## Desktop-notebook switching protocol

- 동시에 두 기기에서 작업하지 않는다. 한 기기의 변경을 commit하고 push한 뒤 다른
  기기에서 이어서 작업한다.
- 세션을 시작할 때 `git status --short --branch`를 실행한다. 작업 트리가 깨끗할
  때만 `git pull --ff-only`를 실행하고, 이후 `HANDOFF.md`와 `STATUS.md`를 읽는다.
- 세션을 마칠 때 코드와 함께 `STATUS.md`와 `HANDOFF.md`를 갱신한다. `HANDOFF.md`에는
  다음 기기에서 바로 실행할 작업 하나와 마지막 검증 결과를 남긴다.
- `git diff --check`와 필요한 테스트를 실행한 뒤 관련 파일만 stage한다. commit과
  push는 사용자가 변경 내용을 확인한 후 수행한다.
- 백그라운드 자동 pull, 자동 commit, 자동 push는 사용하지 않는다. 동기화 스크립트는
  작업 트리가 더럽거나 fast-forward가 불가능하면 변경 없이 중단해야 한다.
- push가 완료되지 않은 기기에서는 작업이 인계된 것으로 표시하지 않는다. 다른 기기는
  원격 commit을 확인한 뒤 작업을 시작한다.

### Session commands

작업을 시작할 때 다음 명령을 사용한다. 작업 트리가 깨끗할 때만 fast-forward pull을
수행하고 현재 상태와 인계 내용을 출력한다.

```bash
./scripts/session-start.sh
```

작업 하나를 완료하거나 다른 기기로 전환할 때 `STATUS.md`와 `HANDOFF.md`를 먼저
갱신한 다음, commit 메시지와 이번 작업에 포함할 파일을 명시한다. 선택하지 않은 변경이
남아 있으면 스크립트는 commit과 push를 수행하지 않는다.

에이전트는 작업이 성공적으로 완료되고 검증됐으며 작업 트리에 관련 없는 변경이 없을 때
이 스크립트를 실행한다. 안전 조건을 만족하지 못하면 push하지 않고 이유와 남은 변경을
보고한다.

```bash
./scripts/session-finish.sh \
  "docs: update Stage 1 progress" \
  docs/SPEC.md STATUS.md HANDOFF.md
```

## Build and verification

- 정확한 Linux 버전과 kernel config를 확인하지 않은 상태에서 호환성을 단정하지 않는다.
- 실행하지 않은 빌드나 테스트를 성공했다고 표현하지 않는다.
- 실패한 명령도 원인 분석에 필요하면 `STATUS.md` 또는 handoff에 기록한다.
- 가능하면 최소한 다음 경로를 검증한다.

  1. 대상 커널 또는 모듈 빌드
  2. RP2040 USB enumeration
  3. Stage 1 trigger
  4. enumeration 이후 custom driver matching과 `probe()` 실행
  5. Stage 2 primitive
  6. 반복 실행 안정성
  7. test flag 획득

환경 또는 하드웨어가 없어 검증하지 못한 항목은 `NOT TESTED`로 표시한다.

## CTF and secret handling

- 작업 범위는 이 CTF 저장소와 명시적으로 허가된 로컬/VM 환경으로 제한한다.
- 실제 제3자 시스템이나 허가되지 않은 장치를 대상으로 테스트하지 않는다.
- 저장소와 문서에는 실제 운영 flag를 기록하지 않는다.
- 예시 값은 `KAPO{test_flag}`처럼 명백한 테스트 값만 사용한다.
- `docs/SPEC.md`, 공식 solve, exploit과 내부 검증 자료는 참가자 배포본에 포함하지 않는다.

## Completion report

작업을 마칠 때 다음을 간단히 보고한다.

1. 변경한 파일과 변경 목적
2. 실행한 테스트와 결과
3. 검증하지 못한 항목
4. 설계에 영향을 주는 결정 또는 새 가설
5. 다음 작업자가 이어서 할 일

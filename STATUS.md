# 프로젝트 진행 상태

최종 갱신: 2026-09-05 KST

## 완료

- Linux 6.8 USB 드라이버 소스 기준본을 `usb/` 아래에 가져왔다.
- 초기 문제 설계를 `docs/SPEC.md`에 기록했다.
- 데스크톱-노트북 Git 인계 절차와 안전한 세션 스크립트를 추가했다.
- 세션 스크립트가 저장소별 GitHub Desktop Git 실행 파일을 사용해 인증된 동기화를
  수행할 수 있다.
- Stage 1 short-configuration heap disclosure를 `usb/core/message.c`와
  `usb/core/config.c`에 구현했다.
- host 회귀 모델로 parser가 실제 수신 길이 안에서만 동작하고, raw descriptor
  출력에는 grooming된 canonical pointer가 포함되는 것을 확인했다.
- KASAN, SLUB debugging, `dummy_hcd`, Raw Gadget을 포함한 QEMU Linux 6.8.12
  guest에서 short configuration disclosure를 재현했다. 광고된 1024-byte accounted
  allocation이 해제된 기본 pipe ring을 재사용하며, enumeration이 vendor interface까지
  진행되는 동안 잔여 vmemmap `struct page *` 값이 sysfs로 유출됐다. 5/5회 확인했다.
- 별도의 32-byte `seq_operations` grooming mode에서 `proc_single_show`의 상위
  7바이트가 유출됐다. 알려진 symbol offset으로 덮인 하위 1바이트를 복원해 KASLR
  slide를 계산할 수 있다. 5/5회 확인했다.
- 최소 `kapo_ctf` driver가 interface `ff/42/01`에 bind된다. Stage 1 short response
  이후 `custom_probe()`가 실행되는 것을 QEMU runtime log로 확인했다.
- Stage 2는 `/dev/kapo_ctf0`을 노출한다. ioctl은 record count 128을 읽고 record
  포인터 크기를 element 크기로 잘못 사용해 1024-byte buffer를 할당한 뒤 2048-byte
  record array를 수신한다. QEMU에서 의도한 write가 KASAN slab-out-of-bounds와 SLUB
  redzone 손상으로 각각 검출됐다.
- custom driver source의 의도적 취약점 주석과 이중 포인터 단서를 제거하고 평범한 record
  배열 allocation의 `sizeof` 실수로 표현했다. Linux 6.8.12 KASAN object에서 allocation은
  `count * 8`, bulk receive는 `count * 16`으로 유지됨을 확인했다. 변경 후 KASAN QEMU에서도
  ioctl의 2048-byte 반환과 slab-out-of-bounds write를 다시 확인했다.
- Raw Gadget 검증 프로그램은 인자 없음과 `seq` 모드의 기존 `A` bulk response를 유지하면서,
  명령행 파일에서 정확히 2048바이트를 읽어 Stage 2 bulk response로 보낼 수 있다. default와
  `seq` 양쪽에서 파일 payload를 사용한 QEMU ioctl이 2048을 반환했다.
- redzone 없는 QEMU trace에서 해제한 live-pipe spray slot
  `ffff8880041fc800`을 Stage 2 allocation이 정확히 재사용했다. 2048-byte receive가
  다음 live ring `ffff8880041fcc00`까지 넘어가는 것을 확인했다.
- target-equivalent Linux 6.8.12 build header를 해당 build의 generated configuration과
  함께 컴파일해 `struct pipe_buffer`의 40-byte 크기와 `page=0`, `offset=8`, `len=12`,
  `ops=16`, `flags=24`, `private=32` byte offset을 산출했다.
- `scripts/make-stage2-pipe-payload.c`는 첫 1024 bytes를 zero로 보존하고 byte 1024의
  인접 `pipe_buffer` entry 하나에 runtime 입력 `page`, `ops`, `offset`, 고정 `len=1`,
  `PIPE_BUF_FLAG_CAN_MERGE`, `private=0`을 little-endian으로 pack한 정확히 2048-byte
  payload를 새 파일로 생성한다. generator 자체 검사는 prefix, field, tail을 확인한다.
- 최종 권한 상승 설계는 유출한 SUID helper의 file-backed `struct page *`, KASLR로
  계산한 `page_cache_pipe_buf_ops`, `PIPE_BUF_FLAG_CAN_MERGE`를 이용한 page-cache
  overwrite로 선택했다. 이 설계의 runtime 동작은 아직 검증하지 않았다.
- 참가자 배포 방향은 custom driver built-in과 exact target의 `bzImage`, `vmlinux`,
  `System.map`, kernel config, Ubuntu 24.04 rootfs 및 RP2040 firmware template 제공으로
  정리했다. release artifact는 아직 만들지 않았다.

## 진행 중

- Stage 2의 controlled `pipe_buffer`와 SUID helper page-cache write 검증.
- 실제 Linux 6.8 환경과 RP2040을 이용한 Stage 1 leak 검증.
- target kernel/rootfs와 참가자 배포 artifact 확정.

## 다음 작업

1. 작은 SUID helper를 만들고, file-backed page pointer leak과 한 페이지 범위의
   `PIPE_BUF_FLAG_CAN_MERGE` overwrite를 QEMU에서 검증한다.
2. 변조된 helper 실행으로 UID 0과 `KAPO{test_flag}` 획득을 연속 10회 시험한다.
3. 32-byte와 1024-byte short configuration을 한 장치의 두 configuration으로 제공하는
   단일 enumeration leak을 시험한다.
4. 실제 RP2040에서 enumeration, `probe()`, 두 leak, Stage 2와 test flag 획득을 검증한다.
5. 검증된 exact kernel/rootfs로 참가자 배포 bundle을 만든다.

## 검증 결과

- 세션 스크립트 문법: PASS
- dirty working tree pull 방지: PASS
- GitHub Desktop 인증 push: PASS
- Stage 1 host poisoned-allocation 모델: PASS
- Linux 6.8 `drivers/usb/core/config.o`, `message.o` 빌드: PASS
- Stage 1 회귀 모델 5회 반복: PASS
- 변경 diff checkpatch: 오류 0, 경고 0. USB 표준 `wTotalLength` 필드에 대한 예상된
  camel-case check 1건만 존재
- `flex` 없이 수행한 첫 Linux 6.8 빌드: FAIL (빌드 도구 누락)
- 사용 가능한 libelf 개발 파일 없이 수행한 두 번째 빌드 준비: FAIL
- KASAN 및 `slub_debug=FZ`를 사용한 QEMU Linux 6.8.12 부팅: PASS
- Raw Gadget 25/1024-byte short configuration enumeration: PASS
- sysfs raw descriptor 길이(18-byte device + 1024-byte configuration): PASS
- 해제한 pipe-ring 주소를 USB buffer가 정확히 재사용한 runtime kmalloc trace: PASS
- 해제한 kmalloc-1k pipe ring의 vmemmap `struct page *` 유출 5회: PASS
- 해제한 32-byte `seq_operations`의 상위 7-byte `proc_single_show` 유출 5회: PASS
- 해제한 pipe ring의 `anon_pipe_buf_ops` 유출: FAIL (`pipe_buf_release()`가 `ops`를
  지우므로 이 grooming lifetime을 text pointer 유출에 사용하지 않는다)
- Linux 6.8.12 `customdriver.o` 빌드: PASS
- 최종 `customdriver.o` 재빌드: 임시 libelf include/library 경로 없이 실행한 첫 명령은
  FAIL, 경로를 지정한 수정 명령은 PASS
- Raw Gadget interface `ff/42/01` bind 및 `custom_probe()` 실행: PASS
- RP2040 enumeration 및 hardware leak 시험: NOT TESTED
- Stage 2 device node, vendor count request, bulk transfer: PASS
- Stage 2의 1024-byte allocation 대상 2048-byte KASAN write 검출: PASS
- Stage 2 SLUB right-redzone overwrite 검출: PASS
- Stage 2의 정확한 freed-slot 재사용 및 인접 live-pipe 도달: PASS
- primitive 단서 완화 후 Linux 6.8.12 `customdriver.o` 재빌드: PASS
- primitive 단서 완화 후 object의 `count * 8` allocation 및 `count * 16` receive: PASS
- Raw Gadget payload 프로그램의 `-Wall -Wextra -Werror` 정적·동적 빌드: PASS
- Raw Gadget default/`seq` 기존 CLI와 파일 payload CLI 검사: PASS
- 2047/2048/2049-byte payload, 없는 파일, 디렉터리 read 오류와 잘못된 인자 처리: PASS
- primitive 단서 완화 후 KASAN QEMU Stage 2 runtime regression: PASS (default와 `seq`
  파일 payload 모두 `trigger-stage2`가 2048 반환, slab-out-of-bounds write 재검출)
- target-equivalent Linux 6.8.12 `struct pipe_buffer` layout compiler extraction: PASS
  (40 bytes; `page=0`, `offset=8`, `len=12`, `ops=16`, `flags=24`, `private=32`)
- Stage 2 pipe-entry payload generator 동적·정적 `-Wall -Wextra -Werror` build: PASS
- Stage 2 pipe-entry payload generator 2048-byte 길이, zero prefix/tail, little-endian
  field packing과 `page=0`, `ops=0`, 음수 주소, offset 4095 이상, 숫자가 아닌 입력 거부:
  PASS
- layout verifier Linux 6.8.12 source/output, matching `source` link 및 target `.config`
  SHA-256 불일치 거부: PASS
- 생성 pipe-entry payload를 Raw Gadget 파일 입력으로 보낸 Linux 6.8.12 KASAN QEMU
  Stage 2 ioctl: PASS (2048-byte 반환 및 기존 allocation-boundary report). 이 시험은
  live entry lifetime과 page-cache write를 검증하지 않는다.
- Stage 2의 조작된 pipe metadata 및 page primitive: NOT TESTED
- SUID helper file-backed page pointer leak: NOT TESTED
- `page_cache_pipe_buf_ops`와 `PIPE_BUF_FLAG_CAN_MERGE` page-cache write: NOT TESTED
- 변조된 SUID helper를 통한 UID 0 및 test flag 획득: NOT TESTED
- 두 configuration을 이용한 단일 enumeration leak: NOT TESTED
- 참가자용 built-in target 및 release bundle: NOT BUILT

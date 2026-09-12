# YOASOBI Challenge Specification

> 출제자 및 개발 에이전트 전용 문서다. 참가자 배포본에는 포함하지 않는다.

## 1. 문제 개요

- 분야: Pwn / Kernel
- 대상: Linux 6.8.12 x86-64 QEMU guest
- 공격자 권한: UID/GID 65534
- 목표: 커널 취약점을 이용해 root-owned `/flag` 읽기
- 입력 장치: organizer-owned trusted USB emulator와 `dummy_hcd`
- 취약점 경로: USB core Stage 1 disclosure → custom driver Stage 2 overflow

참가자는 공개 initramfs의 `kapo{test_flag}`로 exploit을 개발한다. 대회 서버는 TCP
연결마다 실제 flag가 든 disposable QEMU를 시작하고 UID 65534 serial console을 제공한다.
참가자가 업로드한 ELF는 세션별 `/work/exploit`로만 전달하며 host에서 실행하지 않는다.

## 2. Trusted USB emulator 규약

Emulator는 guest에서 root-owned mode `0700`으로 실행되며 USB가 연결되지 않은 상태로
대기한다. USB transport, descriptor, reconnect, Stage 전환과 driver가 보낸 transfer
context 기반 Stage 2 payload 생성만 담당한다. Heap grooming과 flag 접근은 하지 않는다.

참가자 exploit은 각 준비가 끝날 때 `/tmp/yoasobi-ready`에 `ready` 한 줄을 쓴다.

1. 첫 enumeration 전
2. `KAPO_NEXT_STAGE` ioctl과 seq grooming 후
3. `KAPO_NEXT_STAGE` ioctl과 Stage 2 준비 후

Stage 2의 live pipe spray와 hole 생성은 마지막 USB 장치가 bind된 뒤, 취약한 ioctl을
호출하기 직전에 수행한다.

## 3. 의도한 취약점

### Stage 1 — USB short configuration disclosure

- 위치: `usb/core/message.c`, `usb/core/config.c`
- Device는 configuration header에서 큰 `wTotalLength`를 광고하지만 실제 body는
  25바이트만 반환한다.
- Kernel은 광고 길이로 `bigbuffer`를 할당한다.
- CONFIG descriptor buffer를 전부 초기화하지 않고 parser에는 실제 수신 길이만
  전달한다.
- Raw descriptor reader는 광고 길이를 사용하므로 수신되지 않은 tail의 stale slab
  data가 user space에 노출된다.

### Stage 2 — Custom driver heap overflow

- 위치: `usb/class/customdriver.c`
- Device node: `/dev/kapo_ctf0`
- Trigger: `KAPO_FETCH_RECORDS` ioctl
- Driver는 `kmalloc_array(count, sizeof(batch.records), ...)`로 포인터 크기만큼 할당하지만
  bulk receive 길이는 `count * sizeof(*batch.records)`로 계산한다.
- QEMU emulator의 `count = 65`에서 allocation request는 520바이트로 kmalloc-1k 객체를
  받고 bulk receive는 1040바이트이므로 다음 객체의 첫 16바이트를 덮는다.
- `context.ops`는 runtime `page_cache_pipe_buf_ops`와 정확히 일치해야 한다.

## 4. 의도한 exploit chain

1. `/kapo`의 file-backed page가 든 pipe ring을 spray하고 일부를 해제한다.
2. `helper` Stage의 1024-byte short configurations에서 반복되는 `struct page *`를 찾는다.
3. `/proc/self/stat`을 열어 `seq_operations`를 spray하고 일부를 해제한다.
4. `seq` Stage의 32-byte short configurations에서 `proc_single_show`를 leak해 KASLR
   slide와 runtime `page_cache_pipe_buf_ops`를 계산한다.
5. 마지막 Stage가 bind된 뒤 kmalloc-1k pipe ring을 spray한다. 마지막 live anonymous
   pipe 앞의 ring을 해제해 vulnerable allocation hole을 만든다.
6. Ioctl context에 `/kapo` page, runtime pipe ops와 payload offset을 전달한다.
7. 16-byte overflow로 인접 live `pipe_buffer`의 `page`, `offset`, `len`만 바꾼다.
   기존 `anon_pipe_buf_ops`, `PIPE_BUF_FLAG_CAN_MERGE`, `private`는 유지된다.
8. Pipe write로 `/kapo` page cache에 payload를 기록하고 SUID `/kapo`를 실행한다.
9. Payload는 EUID가 0인지 확인한 뒤 `/flag`를 출력한다.

Target kernel은 resize된 old pipe ring을 free하기 전에 초기화해 Stage 1 seq leak을
생략하는 알려진 shortcut을 차단한다.

## 5. 배포와 신뢰 경계

참가자에게 전달하는 `participant/YOASOBI.tar.gz`에는 다음 항목만 둔다.

- `README.md`
- `SHA256SUMS`
- `run-qemu.sh`
- `kernel/bzImage`
- `rootfs/initramfs.cpio.gz`

참가자 archive에는 실제 flag, `writeup/`, organizer exploit, 이 문서, `STATUS.md`,
`vmlinux`, `System.map`, standalone kernel config를 포함하지 않는다.

Private 서버는 이 저장소의 `kernel/bzImage`와 `server/initramfs.cpio.gz`를
`v1r4m/poka2026`의 `poiri3r/13001/`에 반영해 Docker로 배포한다. 원격 launcher는
base64 ELF 하나를 받은 뒤 disposable QEMU를 시작하며 연결 종료나 timeout 때 session
QEMU와 임시 작업 디렉터리를 폐기한다. Guest 발신 네트워크는 QEMU
`user,restrict=on`으로 차단한다.

## 6. 완료 기준

- 참가자와 private image 모두 UID 65534에서 full chain으로 flag를 획득한다.
- Public archive와 server runtime의 kernel, emulator, SUID target이 flag를 제외하고
  동일하다.
- Local launcher와 server launcher의 CPU, memory, PCI topology와 network backend가
  동일하다.
- Participant archive에 내부 자료와 실제 flag가 없다.
- 최종 exploit이 원격 Jeopardy endpoint에서 `PAGE_CACHE_WRITE_PASS`, UID 0, flag와
  `FULL_CHAIN_PASS`를 출력한다.

현재 public/private QEMU TCG와 Poka Docker 배포 후 원격 endpoint 검증은 PASS다.
KVM은 NOT TESTED다.

# KAPO USB Kernel CTF — Organizer Write-up

> 이 문서는 출제자용 핵심 풀이다. `participant/testkernelctf.tar.gz`에 포함하지 않는다.

## 1. 한 줄 요약

USB short configuration으로 커널 heap의 `struct page *`와 KASLR pointer를 두 번
유출한 뒤, custom USB driver의 크기 계산 오류로 인접 `pipe_buffer`의
`page/offset/len` 16 bytes만 바꾼다. 이 파이프에 쓰면 SUID helper의 page cache가
변조되고, helper 실행 후 UID 0으로 `/flag`를 읽는다.

## 2. 신뢰 경계와 역할

- 참가자 exploit은 guest UID 65534에서 helper/seq/pipe grooming, leak 판독,
  주소 계산과 ioctl trigger를 모두 수행한다.
- root USB emulator는 descriptor, disconnect/reconnect, stage 전환과 driver가 보낸
  transfer context로 Stage 2 payload를 만드는 일만 한다. heap grooming과 flag
  접근은 하지 않는다.
- readiness 순서는 `helper` → `KAPO_NEXT_STAGE` → `seq` →
  `KAPO_NEXT_STAGE` → `stage2`다.

[출처: docs/SPEC.md:49] [출처: scripts/trusted-usb-emulator.c:197]

## 3. Primitive 체인

| 단계 | 취약점/객체 | 얻는 primitive | 다음 단계로 넘기는 값 |
|---|---|---|---|
| Stage 1-A | 1024-byte short USB configuration + freed pipe ring | stale heap disclosure | helper page의 `struct page *` |
| Stage 1-B | 32-byte short USB configuration + freed `seq_operations` | kernel text pointer disclosure | KASLR slide, runtime `page_cache_pipe_buf_ops` |
| Stage 2 | 520-byte request allocation + 1040-byte bulk receive | adjacent 16-byte heap overwrite | target `pipe_buffer.page/offset/len` |
| Final | live anonymous pipe + SUID helper | page-cache write | UID 0, `/flag` |

### Stage 1-A — helper page pointer leak

1. 파이프에 anonymous page 두 개를 넣고 SUID helper의 file-backed byte를
   `splice()`해 pipe ring에 helper page pointer를 남긴다.
2. 여러 ring 중 일부를 정상 `close()`해 kmalloc-1k free slot을 만든다.
3. emulator가 구성별 `wTotalLength=1024`를 광고하면서 실제로는 25 bytes만
   보낸다. USB core는 1024-byte `bigbuffer`를 `kmalloc()`한 뒤 25 bytes만
   채우고, raw descriptor 길이는 광고값 1024로 보존한다.
4. `/sys/bus/usb/devices/1-1/descriptors`의 descriptor tail에서 이전 ring의
   `struct page *`를 읽는다. 8개 configuration 중 두 개 이상에 반복되는
   64-byte-aligned vmemmap 후보만 사용한다.

정상 pipe close는 entry를 release하며 `ops = NULL`로 만들기 때문에, 이 단계는
helper page만 주고 KASLR 용 ops pointer는 주지 않는다.

[출처: usb/core/message.c:848] [출처: usb/core/config.c:938]
[출처: usb/core/config.c:942] [출처: usb/core/config.c:951]
[출처: usb/core/config.c:965] [출처: scripts/test-full-chain.c:243]
[출처: docs/SPEC.md:75]

### Stage 1-B — seq leak로 KASLR 해결

1. `KAPO_NEXT_STAGE`로 helper device를 disconnect한다.
2. `/proc/self/stat`을 대량으로 열어 `seq_operations`를 할당하고 일부를
   닫아 kmalloc-32 stale data를 만든다.
3. `seq` readiness를 보내면 emulator가 `wTotalLength=32`, 실제 25-byte
   configuration을 8개 제공한다.
4. tail에 남은 `proc_single_show` runtime pointer에서 정적 symbol 주소를 빼
   KASLR slide를 구하고, runtime `page_cache_pipe_buf_ops`를 계산한다.

[출처: scripts/test-full-chain.c:269] [출처: scripts/test-full-chain.c:449]
[출처: scripts/test-full-chain.c:459]

### Stage 2 — 16-byte adjacent overwrite

custom driver의 핵심 버그는 allocation과 receive가 다른 크기 단위를 쓰는
것이다.

```text
count = 65
allocation request = 65 * sizeof(pointer) = 520 bytes -> kmalloc-1k object
bulk receive       = 65 * sizeof(struct kapo_record) = 1040 bytes
effective overflow beyond the 1024-byte slab object = 16 bytes
```

[출처: usb/class/customdriver.c:95] [출처: usb/class/customdriver.c:169]
[출처: scripts/trusted-usb-emulator.c:21]

참가자는 최종 USB device가 bind된 뒤, 취약한 ioctl 직전에 kmalloc-1k
pipe ring을 spray한다. 하나를 해제해 driver allocation이 들어갈 hole을 만들고,
바로 뒤 live target ring의 첫 entry는 1 byte를 쓴 anonymous pipe buffer로 둔다.

ioctl context는 다음이다.

```text
page     = Stage 1-A에서 leak한 helper struct page *
ops      = Stage 1-B로 계산한 runtime page_cache_pipe_buf_ops
offset   = helper patch offset - 1
reserved = 0
```

driver는 `ops == &page_cache_pipe_buf_ops`를 검증해 seq/KASLR 단계를 강제한다.
그러나 emulator가 실제 overflow payload에 기록하는 것은 다음 16 bytes뿐이다.

```text
pipe_buffer.page   = helper struct page *
pipe_buffer.offset = 0
pipe_buffer.len    = helper patch offset
```

`ops`, `flags`, `private`는 overwrite 범위 밖이다. 따라서 live anonymous entry의
`anon_pipe_buf_ops`, `PIPE_BUF_FLAG_CAN_MERGE`, `private=0`가 그대로 보존된다.

[출처: usb/class/customdriver.c:131] [출처: scripts/trusted-usb-emulator.c:164]
[출처: scripts/test-full-chain.c:315]

### Final — page-cache write와 flag

변조된 anonymous pipe에 payload를 쓰면 `CAN_MERGE`로 인해 `len` 뒤에 이어서
쓰여지고, 실제 page는 helper의 file-backed page로 바뀐 상태다. 결과적으로
helper의 page cache 내 코드가 권한 상승 payload로 바뀐다. mode 4755 helper를
실행해 UID 0을 얻고 `/flag`를 읽는다.

[출처: scripts/test-full-chain.c:499] [출처: scripts/test-full-chain.c:503]

## 4. 의도된 solve 순서

```text
groom helper-backed pipe rings
write "helper" -> read descriptor tail -> helper struct page *
KAPO_NEXT_STAGE -> disconnect
groom seq_operations
write "seq" -> read descriptor tail -> KASLR slide
KAPO_NEXT_STAGE -> disconnect
write "stage2" -> wait for final bind
prepare kmalloc-1k hole + live anonymous pipe target
ioctl(context) -> 16-byte partial overwrite
write(pipe) -> modify helper page cache
exec SUID helper -> UID 0 -> read /flag
```

핵심 timing 조건은 Stage 2 live pipe layout을 final device bind **후**, ioctl
**직전**에 만드는 것이다.

[출처: docs/SPEC.md:38] [출처: scripts/test-full-chain.c:488]

## 5. 막은 unintended shortcut

- 기존 풀이는 `F_SETPIPE_SZ`로 live ring을 resize한 뒤 old ring에서 helper
  page와 ops를 한 번에 유출해 seq leak를 생략했다.
- target patch는 resize된 old ring을 `kfree_sensitive()`로 해제해 stale pointer를
  지운다. 정상 close의 page-only leak는 보존한다.
- Stage 2 driver는 exact runtime `page_cache_pipe_buf_ops`를 요구해 다른 ops로
  KASLR leak를 우회하는 경로를 거부한다.
- 최종 Stage 2를 full `pipe_buffer` 덮기에서 16-byte partial overwrite로 줄여,
  참가자가 유효한 live anonymous entry를 직접 배치하도록 했다.

[출처: kernel-patches/0001-pipe-clear-resized-ring.patch:4]
[출처: usb/class/customdriver.c:131]

## 6. 배포와 현재 판정

- 로컬 배포본은 `kapo{test_flag}`를 넣은 QEMU다.
- 대회에서는 TCP로 base64 ELF를 받고, 실제 flag가 든 disposable QEMU의
  UID 65534 serial shell을 같은 연결에 제공한다.
- organizer chain은 TCG cold boot 5/5에서 test flag를 획득했다. 로컬·원격을 같은
  restricted e1000 장치 배치로 통일한 뒤에도 기존 단일-hole/단일-target exploit이
  TCP fresh boot 5/5와 로컬 launcher 1/1에서 성공했다.
- 최종 partial archive를 받은 독립 AI 3개는 구조를 복원했지만
  971초, 1,637초, 1,787초 내에 flag를 얻지 못했다.
- 난이도는 약 8.5/10으로 평가했다. 외부 독립 완주 표본과 실제
  운영 배포는 아직 release gate로 남아 있다.

[출처: STATUS.md:13] [출처: STATUS.md:25] [출처: STATUS.md:297]

## 7. 핵심 오답 포인트

- `context.ops`가 payload로 덮여진다고 이해하면 안 된다. 이 값은 driver의
  KASLR-leak 검증용이고, live target의 실제 ops는 `anon_pipe_buf_ops`다.
- Stage 2 이전 full-entry 풀이를 그대로 쓰면 1040-byte receive에서 끝나므로
  현재 partial target을 성공시키지 못한다.
- 공개 initramfs는 host에서 추출할 수 있어 emulator를 완전히 숨기지는
  못한다. 실제 flag는 비공개 server initramfs에만 둔다.

[출처: scripts/trusted-usb-emulator.c:156] [출처: scripts/trusted-usb-emulator.c:164]
[출처: docs/SPEC.md:54]

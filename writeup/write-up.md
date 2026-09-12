# YOASOBI Write-up

## 의도한 취약점

1. USB configuration descriptor를 광고된 `wTotalLength` 크기로 할당한 뒤 실제로는 더 짧게 수신한다. CONFIG descriptor 버퍼를 초기화하지 않고 sysfs raw descriptor에는 광고 길이만큼 노출하므로, 뒤쪽 slab 잔여 데이터가 유출된다.
2. `KAPO_FETCH_RECORDS`는 `count * sizeof(batch.records)`만 할당한다. `batch.records`는 포인터라서 65개 요청 시 520바이트만 할당하지만, USB bulk transfer는 `65 * sizeof(struct kapo_record)`, 즉 1040바이트를 기록해 kmalloc-1k 객체 다음 16바이트를 덮는다.

## 실제 풀이 primitive

- 첫 번째 descriptor leak으로 SUID 바이너리 `/kapo`의 file-backed `struct page *`를 구한다.
- 두 번째 descriptor leak으로 `seq_operations`의 `proc_single_show` 주소를 구하고 KASLR slide와 `page_cache_pipe_buf_ops` 주소를 계산한다.
- 16바이트 heap overflow로 인접한 anonymous `pipe_buffer`의 `page`, `offset`, `len`만 바꾼다. 기존 `anon_pipe_buf_ops`와 `PIPE_BUF_FLAG_CAN_MERGE`는 유지되므로, pipe write가 `/kapo`의 page cache에 기록된다.

## 풀이 과정

1. `/kapo`를 `splice()`한 pipe들을 만들고 일부를 해제한 뒤 `helper` Stage를 요청한다. short descriptor의 stale tail에서 반복되는 `struct page *`를 찾는다.
2. `/proc/self/stat`을 대량으로 열고 일부를 닫아 `seq_operations`를 kmalloc-32에 배치한 뒤 `seq` Stage를 요청한다. 유출된 `proc_single_show`로 KASLR slide를 계산한다.
3. 다음 Stage로 이동하고 `stage2`를 요청한다. pipe 객체를 spray한 뒤 마지막 앞 객체를 해제해 취약한 kmalloc-1k 할당이 그 자리를 사용하게 한다.
4. ioctl context에 유출한 `/kapo` page와 계산한 `page_cache_pipe_buf_ops`를 넣는다. overflow가 인접 pipe를 `/kapo`의 payload 위치로 향하게 만든다.
5. 해당 pipe에 payload를 쓰고 SUID `/kapo`를 실행한다. payload는 EUID가 0인지 확인한 뒤 `/flag`를 출력한다.

## 최종 exploit

```sh
make -C writeup
mkdir -p participant/YOASOBI/work
cp writeup/exploit participant/YOASOBI/work/
participant/YOASOBI/run-qemu.sh
```

게스트에서 `/work/exploit`을 실행하면 된다.

## Flag

```text
kapo{d14m0nd_n3v3r_br34ks}
```

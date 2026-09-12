# YOASOBI

제공된 Linux 6.8.12 커널을 UID 65534 권한에서 공격해 `/flag`를 읽으세요.
로컬 이미지의 플래그는 `kapo{test_flag}`입니다.

## 실행

```sh
sha256sum -c SHA256SUMS
mkdir -p work
./run-qemu.sh
```

`work/`는 게스트의 `/work`에 마운트됩니다. 작성한 exploit과 소스 코드를 이곳에
넣으세요. KVM을 사용할 수 없으면 QEMU TCG로 실행됩니다. 게스트의 외부 네트워크
접근은 차단되어 있습니다.

에뮬레이터와 동기화가 필요할 때 `/tmp/yoasobi-ready`에 `ready`를 한 줄로 작성하세요.

대회에서는 TCP endpoint에 Base64로 인코딩한 ELF 한 줄을 전송합니다. 제공된 UID
65534 셸에서 `/work/exploit`을 실행하고 획득한 실제 플래그를 스코어보드에 제출하세요.

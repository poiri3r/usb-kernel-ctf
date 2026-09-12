  1. build-target-kernel.sh
     커널 취약점·드라이버를 수정하고 bzImage를 다시 만들 때 사용합니다.

  2. build-participant-rootfs.sh
     emulator, /init, /kapo, flag를 변경해 initramfs를 재생성할 때 사용합니다.

  3. build-qemu-release.sh
     kernel·rootfs·README를 참가자용 YOASOBI.tar.gz로 묶을 때 사용합니다.

  4. kapo.c
     initramfs 내부 SUID /kapo 바이너리의 소스입니다.

  5. participant-init.sh
     게스트 부팅, FIFO 생성, emulator 실행, UID 65534 셸 시작을 담당합니다.

  6. trusted-usb-emulator.c
     descriptor와 Stage 전환, overflow payload를 제공하는 핵심 emulator입니다.
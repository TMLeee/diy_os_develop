# tools/ — 헤드리스 QEMU 검증 하네스

커널을 눈으로 확인하지 않고 자동으로 검증한다. QEMU 모니터의 `pmemsave`로
VGA 텍스트 버퍼(`0xB8000`)를 덤프해 화면 내용을 텍스트로 읽고, `sendkey`로
셸 명령을 주입한다.

## 실행

Bash 툴이 아니라 **cygwin bash**로 실행해야 한다 (QEMU는 네이티브 Windows
바이너리라 경로 변환이 필요하고, 스크립트가 `cygpath`로 처리한다).

```sh
tools/regress.sh            # 매 커밋 전 회귀 검사 (~40초, 2회 부팅)
tools/verify_all.sh         # 스테이지 경계 전체 검증 (~5분, 20여 회 부팅)
tools/bootcheck.sh 'totalram' 'createtask 2 4'   # 임의 명령 실행 후 화면 덤프
tools/dumpmem.sh 0x7E00 3200 out.bin             # 게스트 물리 메모리 덤프
```

## 환경 변수

| 변수 | 기본값 | 용도 |
|---|---|---|
| `MEM` | 64 | QEMU `-m` 값. RAM 크기별 동작 확인 |
| `BOOT_WAIT` | 5 | 부팅 후 키 입력까지 대기(초) |
| `CMD_WAIT` | 2 | 명령마다 대기(초) |
| `END_WAIT` | 2 | 마지막 명령 후 덤프까지 대기. 소크 테스트에 사용 |
| `SERIAL` | — | 지정하면 COM1 출력을 이 파일로 저장 |
| `INTLOG` | 0 | 1이면 `-d int,guest_errors`. 트리플폴트 추적용, 매우 느림 |
| `HARD_TIMEOUT` | 90 | QEMU 강제 종료 시한(초) |

`bootcheck.sh`의 명령 문자열에서 `~`는 백스페이스로 변환된다.

## 스크립트

- **bootcheck.sh** — 헤드리스 부팅 후 화면을 80x25 텍스트로 출력. 나머지의 기반
- **regress.sh** — 빌드 + 부팅 무결성 + 기존 기능. 부팅 2회로 나누는 이유는
  화면이 25줄뿐이라 명령을 치면 초기화 로그 위쪽이 스크롤로 사라지기 때문
- **verify_all.sh** — 메모리 서브시스템 전체. 페이지 보호 검사는 주소를
  하드코딩하지 않고 `pgtest`가 링커 심볼에서 가져온 값을 쓴다
- **dumpmem.sh** / **scan_e820.py** / **scan_idt.py** — 게스트 물리 메모리를
  꺼내 구조를 해석. IDT 오버런과 E820 테이블을 실측하는 데 썼다

## 커널 쪽 대응 명령

`memmap` `pmemstat` `alloctest` `pgwalk` `pgtest` `slabinfo` `kmalloctest`
`crash div0|pf|gp|ud|wtext|xdata`

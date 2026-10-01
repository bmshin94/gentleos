# GentleOS/16 전수조사 & 활용 전략 정리 📝

> 이 문서는 GentleOS/16 레포지토리를 전수조사하고, 활용 방안·수익화 아이디어까지
> 정리한 한국어 분석 노트입니다.
>
> 작성일: 2026-10-01

---

## 🔗 관련 GitHub / 웹 주소

| 구분 | 주소 |
|---|---|
| **이 포크 (작업 중인 레포)** | https://github.com/bmshin94/gentleos |
| **원본 레포 (upstream)** | https://github.com/luke8086/gentleos |
| **32비트 버전** | https://github.com/luke8086/gentleos32 |
| **공식 웹사이트** | https://luke8086.dev/gentleos16 |
| **HP 200LX 포팅 (기여자)** | https://github.com/l00nix/gentleos-hp200lx |
| 라이선스 | GPLv2 (`LICENSE`) |

### 빌드 도구 / 에뮬레이터 링크
| 도구 | 주소 |
|---|---|
| DOSBox (빌드 필수) | https://www.dosbox.com/ |
| OpenWatcom 1.9 (레포에 포함) | https://www.openwatcom.org/ |
| 86Box (에뮬레이터, 추천) | https://86box.net/ |
| PCem | https://pcem-emulator.co.uk/ |
| MartyPC | https://github.com/dbalsom/martypc |
| v86 (웹 x86 에뮬레이터) | https://github.com/copy/v86 |

### 벤더 에셋 출처
| 에셋 | 출처 |
|---|---|
| 아이콘 | https://icons8.com/ |
| Mona Font | https://github.com/MonadABXY/mona-font |
| Oldschool PC Font Pack | https://int10h.org/oldschool-pc-fonts/ |
| Atari Small | https://hea-www.harvard.edu/~fine/Tech/x11fonts.html |
| Font 4x6 | https://github.com/luizbills/font4x6 |

---

## 1. 이게 뭐하는 건가? (한 줄 결론)

**AI 도구가 아니다.** 1980~90년대 **16비트 PC(8086/286 등)에서 실제로 부팅되는
직접 만든 운영체제(Hobby OS)** 의 전체 소스코드다.

- 정식 이름: **GentleOS/16**
- 제작자: luke8086
- 코드량: C + 어셈블리 **약 10,800줄** (vendor 제외)
- 전체 용량: 59MB (이 중 35MB는 레포에 포함된 1990년대 툴체인)
- 라이선스: **GPLv2**

같은 제작자의 32비트 버전(GentleOS/32)을 더 오래된 하드웨어용으로 단순화한 버전이다.

---

## 2. 폴더별 전수조사 결과

### `boot1/` — 1단계 부트로더 (512바이트)
- `boot1.s` — NASM 어셈블리, `[cpu 8086]`, `[org 0x7c00]`
- BIOS가 MBR 512바이트를 `0x7c00`에 로드 → 세그먼트/스택 세팅
- `INT 13h`로 2단계 로더(4섹터)를 `0x2000:0x100`에 로드, 3회 재시도
- 실패 시 `INT 10h`로 `E` 출력 후 `hlt`
- MBR 파티션 테이블 + `0xAA55` 부트 시그니처를 직접 하드코딩

### `boot2/` — 2단계 부트로더 (C로 작성)
- `start.s` + `main.c` + `boot2.lnk` (링커 스크립트)
- `INT 13h AH=08h`로 디스크 지오메트리 조회 → 실패 시 720K/360K 기본값(9섹터/2헤드) 폴백
- `fix_diskette_param_table()` — 아주 오래된 BIOS(MartyPC 포함) 버그 우회:
  BIOS의 디스켓 파라미터 테이블을 `0x0000:0x0600`으로 복사 후 SPT 값 수정
- 커널 **127섹터(약 64KB)** 를 `0x1000:0x100`에 로드 → 플로피 모터 정지 → 커널로 점프

### `kernel/` — 커널 본체 (13개 파일)
| 파일 | 역할 |
|---|---|
| `main.c` | 부팅 순서 총괄, 매직넘버 `0xf0cacc1a`로 로드 검증, `krn_exit()`로 DOS 복귀 |
| `mem.c` | 메모리 초기화 |
| `heap.c` | **범프(bump) 얼로케이터** — 세그먼트 단위 단방향 할당, `free()` 없음. `0xa000` 초과 시 FATAL + 비프 3회 |
| `keyboard.c` | 8042 키보드 컨트롤러 직접 제어 (또는 BIOS 모드) |
| `timer.c` | PIT 타이머, 기본 **20Hz** (`PIT_FREQUENCY 1193180`) |
| `rtc.c` | RTC. 배터리 사망 기기 대비 기본 날짜 설정 |
| `event.c` | **32칸 링버퍼 이벤트 큐** (KEY_DOWN / KEY_UP / TIMER_TICK) |
| `vga.c` | CGA/VGA 드라이버. `INT 10h AL=04h` → **320×200 4색 모드**. 4가지 색 테마 |
| `speaker.c` | PC 내장 스피커 (비퍼) |
| `isr.s`, `lock.c` | 인터럽트 핸들러 / 인터럽트 잠금 |
| `debug.c` | UART(COM1) 디버그 출력 |

### `lib/` — 자체 표준 라이브러리 (libc 미사용)
`printf.c`(11KB), `string.c`, `math.c`, `rand.c`, `sleep.c`, `time.c`, `bios.c`, `cpu.s`, `key.c`

### `gui/` — 그래픽 엔진
- **`surface.c`** — 핵심. 두 가지 최적화가 압권:
  1. **더티 렉트(dirty rect)**: 변경 영역만 재렌더 (최대 8칸 추적, 자동 병합)
  2. **룩업 테이블**: `gui_surface_byte_expansions[256]` — 1bpp 흑백 → 2bpp CGA 픽셀 변환을
     부팅 시 256가지 전부 미리 계산
- `window.c` / `button.c` / `rect.c` / `grid.c` / `status.c` — 위젯 시스템
- `card.c` — **카드게임 공용 엔진** (프리셀·클론다이크·블랙잭 공유)
- `app.c` — 앱 런타임. 앱 상태를 `gui_app_shared_buffer[1536]` **1.5KB 공용 버퍼 하나**에 담아 메모리 절약
- `main.c` — 메인 이벤트 루프

### `apps/` — 내장 앱 16개
- 시스템: `launcher`(5×3 그리드 홈), `clock`, `calendar`, `fonts`, `keys`, `sounds`, `setup`
- 게임 9종: **지뢰찾기, 짝맞추기, 마작, 스네이크, 테트리스, 2048, 프리셀, 클론다이크, 블랙잭**
- 앱 인터페이스: `app_st` 구조체에 `on_init / on_show / on_key_down / on_key_up / on_tick / on_close` 콜백만 등록

### `tools/` — Perl 코드 생성기 9개 (`AUTOGEN` 실행 순서)
| 순서 | 스크립트 | 역할 |
|---|---|---|
| 1 | `mkcfg.pl` | `_config.h` → `config.h` 생성 |
| 2 | `mkbuild.pl` | build 디렉터리 생성 |
| 3 | `fixlns.pl` | CRLF 줄바꿈 정리 |
| 4 | `mkdata.pl` | **PBM 이미지/폰트 → C 배열 변환** (파일시스템이 없으므로 에셋을 코드에 내장) |
| 5 | `cproto.pl` | **`global` 키워드 스캔 → 헤더(`p_*.h`) 자동 생성** |
| 6 | `genmake.pl` | **Makefile + 링커 스크립트 자동 생성** |
| - | `mkdisks.pl` | 디스크 이미지 생성 (`WMAKE` 단계에서 호출) |
| - | `clean.pl` | 빌드 정리 |

### `vendor/` — 툴체인 통째 포함 (35MB)
OpenWatcom 1.9(12MB), NASM(3.5MB), Perl(19MB), CWSDPMI + 폰트/아이콘 에셋
→ **DOSBox만 있으면 추가 설치 없이 즉시 빌드 가능**

### `assets/` · `doc/`
- `assets/` — PBM 흑백 비트맵 (아이콘 8개, 스프라이트 12개, 마작 타일 수십 개)
- `doc/appimg/` — 앱 스크린샷 16장 (webp)
- `doc/machimg/` — 실기계 사진 3장 (Amstrad PPC512, Toshiba T1100)

### `CLAUDE.md`
이 포크에서 추가한 Claude 페르소나(카리나) 설정 파일. PR #1 (`feat/claude-guide`)로 머지됨.

---

## 3. 부팅 흐름 요약

```
전원 ON
  → BIOS
  → boot1/boot1.s      (512B MBR, INT 13h로 2단계 로드)
  → boot2/main.c       (디스크 지오메트리 조회, 커널 127섹터 로드)
  → kernel/main.c      (mem → heap → keyboard → timer → rtc → vga)
  → gui/main.c         (surface 초기화 → status bar → 런처 실행 → 이벤트 루프)
  → apps/launcher.c    (5×3 아이콘 그리드)
```

---

## 4. 하드웨어 제약 (왜 이렇게 짜야 하는가)

| 항목 | GentleOS/16 | 현대 스마트폰 |
|---|---|---|
| 해상도 | 320 × 200 | ~2500 × 1100 |
| 색상 | **4색** | 1670만색 |
| 메모리 | **64KB** | 8~12GB (약 15만 배) |
| CPU | 4~8MHz 1코어 | 3000MHz 8코어 |
| 파일시스템 | 없음 | 있음 |
| 네트워크 | 없음 | 있음 |
| 멀티태스킹 | 없음 (앱 1개) | 있음 |

---

## 5. 설치 및 사용법

### 방법 A: 직접 빌드 (공식)
```bash
git clone https://github.com/bmshin94/gentleos
cd gentleos
# DOSBox 설치: brew install --cask dosbox-x  /  apt install dosbox
```
DOSBox 안에서:
```dos
Z:\>MOUNT C .
Z:\>C:
C:\>ENV          # env.bat — PATH/INCLUDE/WATCOM 세팅
C:\>AUTOGEN      # Perl 코드 생성 7단계
C:\>WMAKE        # 컴파일 + 링크 + 디스크 이미지 생성
C:\>GENTLEOS.COM # DOS에서 즉시 실행 (Shift-Q로 종료)
```

### 빌드 산출물
| 파일 | 용도 |
|---|---|
| `gentleos.com` | DOS에서 바로 실행 (개발 중 빠른 테스트) |
| `disk.img` | 최소 디스크 이미지 |
| `fd720.img` | 720KB 플로피 |
| `fd1440.img` | 1.44MB 플로피 (가장 범용) |
| `web.img` | 색 반전 버전 (웹 데모용) |

### 방법 B: 에뮬레이터 부팅
```bash
qemu-system-i386 -fda fd1440.img -boot a
```
또는 86Box / PCem / MartyPC에 `fd1440.img` 마운트

### 방법 C: 실제 옛날 PC
`fd1440.img`를 실물 플로피로 구워 8086/286 PC에 꽂으면 실제 부팅됨

### 빌드 속도 팁
- DOSBox 설정 `[cpu]` 섹션에 `cycles=fixed 99999`
- `WMAKE TC=1` — Turbo C 2.01 사용 (`C:\TMP\TC` 설치 필요, 훨씬 빠름)

### 설정 커스터마이징 — `_config.h`
```c
#define DEFAULT_VGA_THEME 1       // 0~3 (Gray/Green/Blue/Cyan)
#define DEFAULT_COLORS_INVERTED 0 // 흑백 반전
#define DEBUG_KEYBOARD 0
#define USE_BIOS_KEYBOARD 0       // HP 200LX 등 8042 없는 기기용
#define DEBUG_TO_UART 0           // COM1 디버그 출력
#define DEFAULT_YEAR 2026         // RTC 배터리 사망 대비
```

### 조작법
| 키 | 동작 |
|---|---|
| 방향키 | 커서 이동 |
| Space / Enter | 앱 실행 |
| ESC | 런처로 복귀 |
| Shift-Q | DOS로 종료 |

---

## 6. 플러그인? 스킬? MCP? → **전부 아님**

| 구분 | GentleOS/16 |
|---|---|
| Claude 플러그인 | ❌ (`plugin.json` 없음) |
| Claude 스킬 | ❌ (`SKILL.md` 없음) |
| MCP 서버 | ❌ (`.mcp.json` 없음, 네트워크 코드 0줄) |
| Node/Python 프로젝트 | ❌ (`package.json` 없음) |
| **16비트 베어메탈 Hobby OS** | ✅ |

정확한 호칭: **"Bare-metal 16-bit Hobby OS"** / **"레트로 컴퓨팅 교육용 OS 프로젝트"**

유일한 AI 관련 파일은 이 포크에서 추가한 `CLAUDE.md`(카리나 페르소나)뿐이다.

---

## 7. API 토큰 필요? → **불필요 (0원)**

- TCP/IP 스택이 아예 없음 → 인터넷 연결 자체가 불가능
- 외부 인터페이스는 `INT 13h`(디스크), `INT 10h`(화면), UART(시리얼)뿐
- 클라우드 서비스 의존 0, npm/pip install조차 불필요 (툴체인 레포 포함)
- 유일한 비용: 이 코드를 Claude와 함께 분석할 때의 Claude 사용료 (코드 자체는 무료)

---

## 8. AI 에이전트 구축에 도움이 되는가?

### 직접적으로는 ❌
LLM API 호출, 벡터 DB/RAG, Function Calling, Python/TS, 비동기 처리 — 전부 없음.
이걸로 AI 에이전트를 만들 수는 없다.

### 간접적으로는 ⭐⭐⭐⭐⭐ (설계 패턴이 놀랍게 일치)

| GentleOS 패턴 | AI 에이전트 대응 개념 |
|---|---|
| `gui/main.c`의 `while(1) { event_wait → handle → flush }` | **에이전트 이벤트 루프** |
| `app_st { name, icon, on_init, on_key_down, ... }` 등록 | **툴(Tool) 레지스트리** — 이름+설명+핸들러 |
| `gui_app_shared_buffer[1536]` 공용 버퍼 재사용 | **컨텍스트 윈도우 관리** — 제한 공간에 뭘 넣고 버릴지 |
| 더티 렉트 (변경 영역만 재렌더) | **증분 업데이트 / 스트리밍 / React Virtual DOM** |
| `cproto.pl`의 `global` 스캔 → 헤더 자동 생성 | **데코레이터 → 툴 스키마 자동 생성** |
| `krn_heap_alloc()` 범프 할당 + OOM 시 FATAL | **리소스 상한 관리 / 백프레셔** |

**결론: AI 에이전트를 "만드는 법"은 안 가르쳐주지만, "잘 만드는 법"은 가르쳐준다.**

---

## 9. React / PHP로 만들 수 있는가?

### OS 자체 → ❌
부트섹터 기계어, `INT 13h`, 인터럽트 핸들러, 세그먼트 메모리 조작 — 브라우저 샌드박스에서 불가능.

### 웹 클론 / 에뮬레이터 → ✅ 완전 가능

**방법 A — v86으로 진짜 OS를 브라우저에서 부팅**
```jsx
import { V86 } from "v86";

function GentleOSPlayer() {
  const screenRef = useRef(null);
  useEffect(() => {
    const emulator = new V86({
      wasm_path: "/v86.wasm",
      memory_size: 32 * 1024 * 1024,
      screen_container: screenRef.current,
      fda: { url: "/fd1440.img" },  // 빌드 산출물
      autostart: true,
    });
    return () => emulator.destroy();
  }, []);
  return <div ref={screenRef} className="gentleos-screen" />;
}
```
레포에 `web.img`(색 반전판)가 있는 것은 제작자도 웹 데모를 염두에 둔 흔적.

**방법 B — React로 UI/게임 클론 (실용적, 추천)**
```tsx
// app_st 구조체를 TypeScript로 그대로 이식
type App = {
  name: string;
  icon: string;
  tickFrequency: number;
  onInit?: () => void;
  onKeyDown?: (code: string, mods: number) => void;
  onTick?: () => void;
};

// kernel/vga.c의 테마 값을 그대로 사용
const THEMES = {
  gray:  { fg: "#cccccc", bg: "#000000" },
  green: { fg: "#67a353", bg: "#000000" },
  blue:  { fg: "#00b0e8", bg: "#000000" },
  cyan:  { fg: "#55ffff", bg: "#002041" },
};
```
게임 로직이 이미 구조체 기반으로 깔끔해서 **거의 1:1 번역 가능**.

**방법 C — PHP 백엔드로 원본에 없는 기능 추가**
```php
// 원본은 파일시스템이 없어 점수 저장 불가 → 웹이 해결 (핵심 차별점)
Route::post('/api/scores', fn(Request $r) => Score::create([
    'game' => $r->game, 'score' => $r->score,
    'player' => $r->player, 'theme' => $r->theme,
]));

Route::get('/api/leaderboard/{game}', fn($g) =>
    Score::where('game', $g)->orderByDesc('score')->limit(100)->get());

// 리플레이: 키 입력 시퀀스만 저장하면 결정론적 재현 가능
Route::post('/api/replays', fn(Request $r) =>
    Replay::create(['game' => $r->game, 'inputs' => $r->inputs]));
```

**추천 아키텍처**
```
React + TypeScript (프론트)
 ├─ <RetroScreen/>    320×200 + CRT 스캔라인
 ├─ <Launcher/>       5×3 아이콘 그리드
 ├─ <Tetris/> <Snake/> <Mines/> ... 게임 9종
 ├─ <ThemeSwitch/>    4색 테마
 └─ <V86Emulator/>    "진짜 OS 부팅" 모드
        ↕ REST / WebSocket
PHP (Laravel) + MySQL
 ├─ 랭킹 / 리더보드
 ├─ 리플레이 저장·재생
 └─ 회원 / 업적 / 일일 챌린지
```

**레트로 CSS**
```css
.retro-screen {
  image-rendering: pixelated;
  font-family: "Px437 IBM VGA 8x16", monospace;
  aspect-ratio: 320 / 200;
}
.retro-screen::after {  /* CRT 스캔라인 */
  content: ""; position: absolute; inset: 0; pointer-events: none;
  background: repeating-linear-gradient(0deg, rgba(0,0,0,.15) 0 1px, transparent 1px 3px);
}
```

---

## 10. 유튜브 강의 영상 제작 가능성 → ✅ 매우 적합

### 적합한 이유
- 시각적 임팩트: "1980년대 컴퓨터가 켜지는 순간" = 높은 클릭률
- 적당한 분량: 1만 줄 → 시리즈로 완주 가능 (리눅스 3000만 줄은 불가능)
- 낮은 진입장벽: DOSBox만 있으면 시청자도 따라할 수 있음
- 에셋 준비됨: 앱 스크린샷 16장 + 실기계 사진 3장
- 한국어 OS 개발 콘텐츠 희소 → 블루오션
- GPLv2 — 해설/설명은 자유 (크레딧 표기 필수)

### 추천 커리큘럼 (12부작)
| # | 제목 | 길이 |
|---|---|---|
| 0 | 1980년대 PC에 내가 만든 OS를 넣어봤다 (훅) | 8분 |
| 1 | 컴퓨터는 어떻게 켜지는가 — 512바이트의 비밀 | 15분 |
| 2 | 부트로더 2단계: 왜 두 번 나눠 로드할까 | 18분 |
| 3 | 커널의 첫 숨: 메모리와 힙 (`free()` 없는 세상) | 20분 |
| 4 | 키보드는 어떻게 작동하나 — 인터럽트 입문 | 20분 |
| 5 | 시간을 다루다: PIT 타이머와 RTC | 15분 |
| 6 | 화면에 점 하나 찍기 — VGA 320×200 4색 (하이라이트) | 22분 |
| 7 | GUI를 맨손으로: 창·버튼·상태바 | 20분 |
| 8 | 1.5KB로 앱 15개 돌리기 | 18분 |
| 9 | 테트리스를 OS 위에서 직접 만들기 (최고 인기 예상) | 25분 |
| 10 | Perl이 코드를 써준다 — 자동 코드 생성 | 15분 |
| 11 | 진짜 플로피로 구워서 고물 PC 부팅 (피날레) | 20분 |

### 제작 팁
- DOSBox + OBS 녹화, 보간(스무딩) 끄기 → 픽셀 선명하게
- 실기계 B컷 촬영 필수 (에뮬레이터만으로는 임팩트 반감)
- 86Box/MartyPC 디버거로 레지스터 실시간 변화 시각화
- 코드는 한 번에 3~5줄만, 애니메이션 강조
- "이 코드는 이렇다"(X) → "컴퓨터는 어떻게 켜질까?"(O) — '왜'를 먼저

### 주의
- GPLv2 + 원작자(luke8086) 크레딧 표기, 더보기란에 원본 레포 링크 필수
- 벤더 폰트/아이콘은 별도 라이선스 → "에셋 판매"는 불가

---

## 11. 수익화 아이디어 (상세)

### 법적 안전지대
| 행위 | 가능 여부 |
|---|---|
| 코드 해설·강의 | 🟢 완전 자유 (크레딧만) |
| 수정 후 무료 배포 | 🟢 가능 (소스 공개 필수) |
| 수정 후 유료 배포 | 🟡 조건부 (소스 공개 의무 → 사실상 어려움) |
| 클로즈드 소스 판매 | 🔴 GPLv2 위반 |
| 직접 작성한 웹 클론 판매 | 🟢 자유 (코드 미복제 시 아이디어는 자유) |
| 벤더 폰트/아이콘 재판매 | 🔴 불가 |

> **황금 규칙: 코드를 팔지 말고, 지식과 경험을 팔 것.**

### 아이디어 1. 유튜브 + 온라인 강의 (최우선 추천)
- Phase 1 (1~2개월): 훅 영상 1편으로 시장 테스트 (비용 0원, 조회수 1만 돌파 여부 체크)
- Phase 2 (3~6개월): 12부작 시리즈 + GitHub 단계별 브랜치(`lesson-01-bootloader` …)
- Phase 3 (6~12개월): 유료 강의 출시 (인플런 ₩99,000 / 유데미 ₩19,000, 15~20시간)

| 항목 | 보수적 | 중간 | 낙관적 |
|---|---|---|---|
| 구독자 | 3천 | 1만 | 3만 |
| 애드센스(월) | ₩10만 | ₩50만 | ₩200만 |
| 강의 수강생(누적) | 100명 | 500명 | 2,000명 |
| 강의 매출 | ₩500만 | ₩2,500만 | ₩1억 |
| 멤버십(월) | ₩5만 | ₩30만 | ₩100만 |
| **1년 총합** | **~₩700만** | **~₩3,500만** | **~₩1.4억** |

### 아이디어 2. 레트로 게임 웹 플랫폼 (React + PHP)
- 게임 9종 React 포팅 + PHP 랭킹/리플레이 (원본은 파일시스템이 없어 점수 저장 불가 → 차별점)
- 차별화: "진짜 OS 부팅 모드"(v86), CRT 효과·비퍼 사운드, 각 게임에서 원본 C 코드 보기 버튼
- 수익: 애드센스 월 ₩20~300만 / 프리미엄(월 ₩2,900) 월 ₩50~500만 / 시즌패스
- 일정(1인 기준 약 3개월):
  | 주차 | 작업 |
  |---|---|
  | 1~2 | 공통 엔진(그리드·키입력·테마) + 스네이크 |
  | 3~4 | 테트리스 + 2048 |
  | 5~6 | 지뢰찾기 + 짝맞추기 |
  | 7~9 | 카드게임 3종 (공통 카드 엔진 활용) |
  | 10 | 마작 |
  | 11~12 | PHP 랭킹/리플레이 백엔드 |
  | 13 | v86 부팅 모드 + 배포 |

### 아이디어 3. 커스텀 레트로 하드웨어 키트
| 상품 | 가격대 |
|---|---|
| 부팅 플로피 디스크 + 레트로 라벨 | ₩15,000 |
| 부팅 가능 SD/CF 카드 | ₩35,000 |
| 올인원 레트로 PC 키트 (중고 486/286 + 설치) | ₩200,000~ |
| HP 200LX 패키지 (`USE_BIOS_KEYBOARD` 포팅 존재) | ₩150,000~ |
| 굿즈 (티셔츠/스티커/포스터, 런처 픽셀아트) | ₩20,000~ |

마진 40~60%, 월 50개 판매 시 월 ₩100~300만. 단 재고/배송/CS 부담.
GPL 안전: 플로피 판매 시 **소스 접근 경로(레포 링크 QR) 동봉 필수**.

### 아이디어 4. 교육용 시뮬레이터 SaaS (B2B)
브라우저에서 부팅 단계별 실행, 레지스터/메모리 시각화, 자동 채점, 교수용 대시보드
- 개인 월 ₩9,900 / 학과 연 ₩300만 / 학원 연 ₩500만
- 수익성 최상, 난이도 최상 (개발 6개월+ & 영업 필요) → 아이디어 1 성공 후 확장

### 아이디어 5. 기술 블로그 + 뉴스레터 (가장 쉬운 시작)
주 1회, 2000~3000자 + 코드 스니펫
- 애드센스 월 ₩5~50만 / 유료 구독(월 ₩4,900 × 200명) 월 ₩98만 / 전자책(₩15,000 × 300부) ₩450만
- 비용 0원, 난이도 최하, **유튜브 대본 재활용 가능**

### 보너스 아이디어
| # | 아이디어 | 수익성 | 난이도 |
|---|---|---|---|
| 6 | OS 개발 1:1 멘토링 (시간당 ₩50,000) | 💰💰 | ⭐ |
| 7 | 기업 기술 세미나 강연 (회당 ₩100~300만) | 💰💰💰 | ⭐⭐ |
| 8 | GentleOS 앱 개발 챌린지 대회 주최 (후원) | 💰 | ⭐⭐⭐ |
| 9 | 레트로 게임 모바일 앱 출시 | 💰💰 | ⭐⭐⭐ |

### 추천 로드맵
```
0~2개월    [5] 블로그 시작 (무료, 리스크 0)
           + [1-Phase1] 유튜브 훅 영상 1편 → 반응 테스트
2~6개월    [1-Phase2] 유튜브 12부작
           + [2] React 웹 플랫폼 개발 착수
6~12개월   [1-Phase3] 유료 강의 출시
           + [2] 웹 플랫폼 런칭 / [3] 굿즈 소량 테스트
12개월+    [4] B2B SaaS 또는 [7] 기업 강연
```

**선정 근거:** ① 리스크 낮은 것부터 ② 콘텐츠 재활용(블로그 → 유튜브 → 강의 교안)
③ 상호 시너지(유튜브↔웹앱) ④ React/PHP 기존 역량 활용

**비추천:** 코드 자체 판매(GPL 위반 위험), NFT/코인(평판 리스크),
처음부터 B2B SaaS(개발 6개월 + 영업력 필요)

---

## 12. 종합 평가

| 관점 | 점수 | 비고 |
|---|---|---|
| 실무 즉시 활용성 | ⭐☆☆☆☆ | 네트워크/FS/멀티태스킹 없음 |
| C 언어 학습 교재 | ⭐⭐⭐⭐⭐ | 포인터·세그먼트·비트연산·인터럽트 |
| 아키텍처 설계 학습 | ⭐⭐⭐⭐⭐ | 이벤트 루프·콜백 레지스트리·상태 관리 |
| 리소스 최적화 사고 | ⭐⭐⭐⭐⭐ | 더티렉트·룩업테이블·공용 버퍼 |
| 빌드 자동화(codegen) 학습 | ⭐⭐⭐⭐ | Perl 기반 헤더/Makefile 자동 생성 |
| 콘텐츠·수익화 소재 | ⭐⭐⭐⭐⭐ | 유튜브/강의/웹앱 모두 적합 |
| AI 에이전트 직접 구축 | ⭐☆☆☆☆ | 불가능 (단 설계 감각은 ⭐⭐⭐⭐⭐) |

**한 줄 요약:** 그 자체로는 수익이 0원이지만, **지식·콘텐츠로 변환하면 큰 가치가 나오는 교육용 금광.**
가장 중요한 첫걸음은 **블로그 글 1편 또는 유튜브 훅 영상 1편**.

---

_이 문서는 Claude Code(카리나 페르소나)와의 전수조사 대화를 정리한 것입니다._

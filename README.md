# BeCompare — x64 어셈블리 구현 (Win32 GUI)

파일·폴더 비교 도구 기능 PRD를 기반으로 **x64 NASM 어셈블리**로 구현한 Windows GUI 비교·병합·동기화 도구입니다.
C 런타임은 `printf`/`malloc`/`qsort` 같은 범용 함수에만 쓰고, 화면·스레드·파일·알고리즘은 모두 어셈블리입니다.

> PRD(공개 정보를 정리한 문서)의 요구사항을 따라 만든 독립 구현입니다.

## 빌드

### 준비물

| 도구 | 용도 | 확인 |
| --- | --- | --- |
| Windows 10 이상 (x64) | 실행·빌드 환경 | |
| NASM 2.16 이상 | 어셈블 (`nasm -f win64`) | `nasm -v` |
| MinGW-w64 (`gcc`, `windres`) | 링크·리소스 컴파일 (UCRT 빌드 권장) | `gcc --version` |
| PowerShell 5.1 이상 | 빌드 스크립트 실행 | |
| Python 3 (선택) | 정적 검사·회귀 검사 | `python --version` |

NASM과 MinGW-w64는 `PATH`에 있어야 합니다. 예: `winget install NASM.NASM`, `winget install BrechtSanders.WinLibs.POSIX.UCRT`.

### 빌드 순서

```powershell
git clone https://github.com/smseven/becompare.git BeCompare
cd BeCompare

powershell -File build-gui.ps1      # gui/*.asm -> build/*.obj -> bcgui.exe (GUI, 메인 프로그램)
powershell -File build.ps1          # src/bc.asm -> bc.exe (이전 단계의 콘솔 도구, 선택)
```

- 성공하면 마지막 줄에 `built bcgui.exe`(또는 `built bc.exe`)가 출력되고 실행 파일이 저장소 루트에 만들어집니다. 중간 산출물은 `build/`에 쌓입니다.
- `build-gui.ps1`은 `gui/`의 모듈을 하나씩 `nasm -f win64`로 어셈블하고, 리소스(`gui/res/app.rc`, 매니페스트)를 `windres`로 컴파일한 뒤 `gcc -mwindows`로 링크합니다. 링크 라이브러리: user32, gdi32, comctl32, comdlg32, shell32, ole32, dwmapi, wininet, gdiplus, uxtheme, shlwapi, version, advapi32.
- 새 모듈을 추가하면 `build-gui.ps1`의 `$optional` 목록에 이름을 넣어야 링크됩니다.

### 검사 (빌드 후)

```powershell
python tools/hazard.py              # invoke 인자 덮어쓰기 / 스택 프레임 16바이트 정렬 정적 검사 -> "hazards: 0" 이어야 함
python tools/test_regressions.py    # 격리된 경로에서 재빌드 + 저장 오류·상태·명령줄 회귀 검사 57개
powershell -File tools/t_smoke.ps1  # 세션 유형 14가지가 3초 이상 살아 있는지 확인 (실제 창을 띄움)
```

### 자주 막히는 곳

- `nasm`/`gcc`/`windres`를 찾지 못함: 설치 후 새 터미널을 열어 `PATH`를 다시 읽게 합니다.
- `cannot open output file ..\bcgui.exe: Permission denied`: 실행 중인 `bcgui.exe`를 종료한 뒤 다시 빌드합니다.
- `symbol ... not defined` (NASM): 새 상수·문자열을 추가한 파일에서 `extern`/`%define`을 확인합니다.
- 화면 캡처형 시험 스크립트(`tools/t_*.ps1`)는 개발 PC의 임시 경로를 가정하므로 그대로 쓰지 말고 참고용으로 보세요.

## 실행

```
bcgui.exe                                   홈 화면 (세션 유형 선택, 최근 세션)
bcgui.exe folder A B                        폴더 비교 (dir, .zip, .bcss 스냅샷, ftp:// 가능)
bcgui.exe text|diff A B                     텍스트 비교
bcgui.exe table|csv A B                     표 비교 (CSV/TSV/XLSX)
bcgui.exe hex A B                           헥스 비교
bcgui.exe picture|pic|image A B             이미지 비교
bcgui.exe merge LEFT RIGHT [BASE] [OUT]     텍스트 병합 (BASE 있으면 3-way)
bcgui.exe foldermerge LEFT RIGHT [BASE] [OUT]  폴더 병합 (3-way)
bcgui.exe version A B                       파일 버전 리소스 비교
bcgui.exe registry KEY_A KEY_B              레지스트리 키 비교 (예: HKCU\Software\X)
bcgui.exe media A B                         오디오 태그 비교 (MP3 / FLAC)
bcgui.exe edit FILE                         텍스트 편집
bcgui.exe file.bcs | file.bcw               세션 / 워크스페이스 열기
bcgui.exe /script file.bcscript             스크립트 실행 (화면 없음)
```

자동화 스위치 (명령줄 인터페이스, AU-04/05):

| 스위치 | 설명 |
| --- | --- |
| `/report out.html` | 폴더 비교 결과를 HTML 보고서로 저장하고 종료. 변경된 텍스트 파일은 `out_files\N.html` 상세 비교 보고서로 링크. 종료 코드 0=동일, 1=차이, 2=오류 |
| `/report out.xlsx` · `/report out.docx` | 확장자가 `.xlsx` / `.docx` 이면 같은 결과를 Excel 통합 문서 / Word 문서로 저장 (파일별 상세 보고서 링크는 HTML에만 있음) |
| `/sync update\|mirror\|twoway` | 비교 후 동기화(확인 창 없음). 미러는 오른쪽에만 있는 항목을 휴지통으로 보냄 |
| `/snapshot file.bcss` | 왼쪽 폴더의 스냅샷(상대 경로·크기·시각·CRC32)을 저장하고 종료 |
| `/exit` | 작업이 끝나면 종료 |
| `/save file.bcs`, `/savews file.bcw` | 열린 세션 / 워크스페이스를 저장 |
| `/shellinstall`, `/shelluninstall` | 탐색기 오른쪽 클릭 메뉴(현재 사용자) 등록 / 해제 |

스크립트 언어 (AU-04): 한 줄에 하나, `;` 또는 `#` 로 주석.

```
set L=%SCRIPTDIR%\left
set R=%SCRIPTDIR%\right
echo comparing %L% and %R%
run folder %L% %R% /report %SCRIPTDIR%\out.html     ; 작업 1개를 화면 없이 실행하고 ERRORLEVEL 설정
if errorlevel 1 goto diff
exit 0
:diff
exit 7
```

로그는 `<스크립트>.log` 에 남고, 종료 코드는 마지막 `exit` 또는 마지막 작업의 값입니다.

VCS 외부 도구 연동 (IN-01), 예: Git

```
git config --global diff.tool becompare
git config --global difftool.becompare.cmd 'bcgui.exe text "$LOCAL" "$REMOTE"'
git config --global merge.tool becompare
git config --global mergetool.becompare.cmd 'bcgui.exe merge "$LOCAL" "$REMOTE" "$BASE" "$MERGED"'
```

## 화면과 기능

| 세션 | 구현 내용 |
| --- | --- |
| **홈** | 모든 세션 유형 카드, 최근 세션(유형·파일·경로·경과 시간, 목록에서 제거), 탭으로 여러 세션 동시 열기, 파일·폴더 끌어놓기(IN-03) |
| **폴더 비교** (FC) | 병합 스캔(백그라운드 스레드, 취소 가능), 접고 펼 수 있는 트리, 상태 색상, 크기·수정 시각·내용·CRC32·속성 기준, 상태 필터(분할 토글 버튼), 이름 제외 필터(**제외 선택 대화상자**, 목록 오른쪽 클릭으로 이름·확장자·위치 제외), **크기 범위(KB)·최근 수정(일) 필터**, 방향 열, 복사 →/←, **이동 →/←**, **이름 바꾸기(F2, 양쪽 동시)**, 삭제(휴지통), 동기화 미리보기 대화상자, HTML 보고서(+ 파일별 상세 보고서 링크), **Excel(.xlsx)·Word(.docx) 내보내기**, ZIP/스냅샷/FTP 읽기 전용 비교 |
| **폴더 동기화** (SY) | 왼쪽→오른쪽 갱신 / 미러 / 양방향, 실행 전 계획 검토 창(작업 목록·필터), 백그라운드 실행 + 일시중지, `.bak` 백업, 삭제는 휴지통 |
| **폴더 병합** (FM) | 3개 폴더(왼쪽·오른쪽·Base)를 파일 단위로 자동 판정(같음 / 왼쪽 변경 / 오른쪽 변경 / 추가 / 삭제 / 충돌), 항목별 수동 채택·제외, 충돌 파일은 3-way 텍스트 병합 세션으로 열어 출력 폴더에 저장, 출력 폴더로 병합 실행 |
| **텍스트 비교** (TC) | 좌우 정렬 비교, 줄 안 차이 강조, 줄 번호 거터·차이 막대·인라인 복사 글리프·차이 미니맵, 공백·대소문자·주석 무시, 인라인 편집, 블록 가져오기, 인코딩/줄바꿈 보존 저장, **구문 강조**(C 계열·스크립트·어셈블리/INI·XML/HTML·SQL/Lua·JSON, 확장자로 자동 선택), **수동 정렬**(양쪽에서 줄을 고르고 `정렬 지정`) |
| **텍스트 편집** | 단일 파일 편집기(인코딩·줄바꿈 보존 저장), 찾기/이전/다음, 바꾸기/모두 바꾸기, 구문 강조, 줄·열 표시 |
| **텍스트 병합** (TM) | 3-way(diff3)·2-way, 충돌 마커와 색상, 충돌 이동, 왼쪽/오른쪽/둘 다 채택, 출력 편집·저장 |
| **표 비교** (TB) | CSV/TSV/**XLSX**(공유 문자열·첫 시트), 행 번호/키 열/**복합 키(2,3)**/전체 열 매칭, 숫자·**날짜(초 단위)** 허용 오차, 무시할 열, 셀 단위 색상, 차이만 보기 |
| **헥스 비교** (HX) | 자체 그리기 창, 다른 바이트 강조, 차이 행만 보기, 차이 이동, 16진 인라인 편집·저장, 찾기, **바꾸기/모두 바꾸기**(같은 길이) |
| **이미지 비교** (PC) | GDI+ 디코딩, 나란히·차이 지도·겹치기, RGB 허용 오차, 회전·뒤집기, 확대·축소 |
| **버전 비교** (VR) | 파일 크기·수정 시각, 고정 버전 정보, FileDescription·CompanyName 등 문자열 리소스 |
| **레지스트리 비교** (RG) | 키·값 재귀 비교(문자열·DWORD·QWORD·이진·다중 문자열), 한쪽에만 있는 키/값 |
| **미디어 비교** (MD) | MP3(ID3v2.2/2.3/2.4, ID3v1)·FLAC(Vorbis 주석) 태그, 오디오 정보, 태그를 제외한 오디오 데이터 CRC32, 표지 유무 |
| **공통** | 세션(.bcs)·워크스페이스(.bcw), 라이트/다크 모드, **설정 대화상자**(글꼴·색·단축키·외부 도구), 3분할 상태 표시줄, 경로 막대 ⇄ 교체 |

단축키(설정에서 변경 가능): `F5` 비교 · `Ctrl+S` 저장 · `Ctrl+W` 탭 닫기 · `Ctrl+Tab` 다음 탭 · `F8`/`Shift+F8` 다음/이전 차이(텍스트·헥스) · `Esc` 취소 · `F2` 이름 바꾸기(폴더)

폴더 비교의 제외 목록과 내보내기:

- **제외 선택…** 버튼: 사용 중인 패턴, 마지막 스캔에서 찾은 폴더·확장자(항목 수 표시), 자주 쓰는 항목(`.git`, `node_modules`, `*.tmp` …)을 체크 목록으로 보여 줍니다. 직접 입력한 패턴도 추가할 수 있고, 확인하면 다시 비교합니다.
- 결과 목록에서 **오른쪽 클릭**: 선택한 항목을 `이름으로 제외` / `확장자로 제외` / `이 위치만 제외`.
- 패턴 규칙: `;` 로 구분, `*` `?` 사용 가능. 이름 패턴(`node_modules`, `*.log`)은 모든 하위 폴더에 적용되고, `\` 로 시작하는 패턴(`\src\build`)은 비교 루트 기준 그 위치의 항목만 제외합니다. 전체 길이는 255자까지입니다.
- **보고서 · 내보내기** 버튼: Excel(.xlsx) / Word(.docx) / HTML 중에서 고릅니다. 상태 필터(같음·다름·왼쪽만·오른쪽만)에서 켜 둔 항목만 내보냅니다. xlsx는 머리글 고정·자동 필터·크기 숫자 서식, docx는 가로 A4 표입니다. 파일은 압축하지 않은 ZIP(OOXML)로 기록합니다.

설정 / 정책 / 외부 도구 (CU-01·CU-02·EX-01):

- 설정은 `%LOCALAPPDATA%\BeCompareAsm\settings.ini` 에 저장됩니다. 글꼴은 다시 시작해야 적용되고, 색·단축키는 즉시 적용됩니다.
- 외부 도구: `[External]` 섹션에 `.확장자=명령줄` (`%1` = 왼쪽 파일, `%2` = 오른쪽 파일). 폴더 비교에서 해당 확장자 파일을 더블클릭/Enter 하면 내부 비교 대신 실행됩니다.
- 관리자 정책: `HKLM\Software\Policies\BeCompareAsm`(또는 HKCU)의 DWORD 값 `DisableSync`, `DisableExternalTools`, `DisableFtp`, `DisableSettings` 가 0이 아니면 해당 기능이 차단됩니다.
- 탐색기 통합: 보기 메뉴 → "탐색기 오른쪽 클릭 메뉴 등록" 으로 `왼쪽으로 선택` / `선택한 항목과 비교` 항목을 추가합니다(현재 사용자, 레지스트리 동사).

## PRD 대비 구현 현황

구현 ✅ / 일부 🟡 / 미구현 ❌. 영역 코드: FC 폴더 비교, TC 텍스트 비교, TB 표 비교, HX 헥스 비교, PC 이미지 비교, MD 미디어 비교, VR 버전 비교, RG 레지스트리 비교, EX 외부 도구, TM 텍스트 병합, FM 폴더 병합, SY 동기화, VF 가상 파일 시스템(원격·특수 저장소), AR 압축 파일, SN 스냅샷, AU 자동화, IN 통합, CU 사용자 정의.

| 영역 | 대상 | 상태 | 비고 |
| --- | --- | --- | --- |
| FC-01 FC-02 FC-03 FC-07 | 폴더 비교 세션: 트리 스캔, 비교 기준, 필터, 결과 처리 | ✅ | 기준: 크기·시각·내용·CRC32·속성 (실행 파일 버전 값은 버전 비교 세션에서 확인). 이름 제외 + 크기/날짜 필터 |
| FC-04 | 파일 속성 비교·필터 | ✅ | Windows 추가 속성(읽기 전용·숨김·시스템·보관·링크(reparse)·압축·오프라인·암호화) 열과 제외 필터. Unix 소유자/그룹은 Windows에 없어 해당 없음 |
| FC-06 | 이름이 다른 항목 정렬 | ✅ | 유니코드 정규화(NFC) 차이, 확장자만 다른 파일, 와일드카드 마스크(`*.txt=*.bak;...`)로 짝을 맞춤. 짝이 맞춰진 항목은 비교·열기·보고서만 가능하고 복사·동기화·삭제·이동·이름 변경에서는 제외 |
| FC-05 | 폴더 비교 보고서 | 🟡 | HTML 보고서 + 파일별 상세 보고서 링크 ✅, Excel(.xlsx)·Word(.docx) 내보내기 ✅(상태별 색, 현재 상태 필터 반영). 인쇄용 레이아웃 ❌ |
| TC-01 TC-03 TC-04 | 텍스트 비교: 줄 비교·정렬, 수동 정렬, 구문 강조 | ✅ | 키워드 목록 기반의 단순 토크나이저 |
| TC-02 | 텍스트 비교 규칙(무시 옵션) | 🟡 | 대소문자·공백·주석·줄 종결 ✅. 정규식, 페이지 머리글, 열, 수동 선택 ❌ |
| TC-05 | Word·PDF·RTF 변환 비교 | ✅ | .docx/.docm 본문, .rtf, .pdf(글꼴 인코딩이 단순한 텍스트만)를 텍스트로 변환해 비교. 변환한 파일은 텍스트로 덮어쓰지 않도록 저장을 막음 |
| TC-06 | 클립보드·찾기/바꾸기·북마크·패치 보기 | ✅ | 클립보드 비교, 비교 화면 찾기/바꾸기, 북마크(Ctrl+F2/F2/Shift+F2), 패치(diff) 파일 좌우 보기(`bcgui patch 파일`) |
| TC-07 | 대체 텍스트·식별자 이름 변경 무시 | ✅ | 식별자 전체를 같은 토큰으로 보는 방식(변수명 변경 무시)과 `old=new` 대체 규칙. 식별자 간 대응 관계까지 검사하지는 않음 |
| TB-01 TB-02 TB-03 TB-04 | 표 비교: 파일 형식, 키 매칭, 셀 허용 오차, 열·행 옵션 | 🟡 | CSV/TSV/XLSX ✅, 복합 키, 숫자·날짜 허용 오차 ✅. HTML 표, 열 순서 변경, 여러 시트 선택 ❌ |
| HX-01 | 헥스 비교·편집 | ✅ | 바꾸기는 덮어쓰기(같은 길이)만 지원 |
| PC-01 | 이미지 비교(차이 지도·겹치기 등) | 🟡 | 교체된 색 무시, 썸네일 확대 ❌ |
| MD-01 | 미디어 비교: 오디오 태그·정보 | 🟡 | MP3(ID3v1/v2)·FLAC(Vorbis 주석)만. MP4/OGG 태그 ❌ |
| VR-01 RG-01 | 파일 버전 리소스 비교, 레지스트리 키 비교 | ✅ | 버전: 고정 정보·문자열 리소스. 레지스트리: 키·값 재귀 비교 |
| EX-01 | 외부 도구 위임 | ✅ | 확장자별 명령줄(`%1`·`%2`), 폴더 비교에서 더블클릭/Enter |
| TM-01~05 | 텍스트 병합: 3-way/2-way, 충돌 해결, 전체 우선 채택 | ✅ | |
| TM-06 | 병합 중 여러 줄 수동 정렬 | 🟡 | 2-way 병합에서 왼쪽·오른쪽 창의 선택 줄(여러 줄이면 첫 줄과 마지막 줄)을 짝지어 다시 병합. 기준 파일이 있는 3-way 병합에서는 지원하지 않음 |
| FM-01 FM-02 FM-03 | 폴더 병합: 3개 폴더 판정·해결·출력 | ✅ | 충돌은 텍스트 병합 세션으로 해결(버튼 또는 행 더블클릭). 이진 파일 충돌은 채택 방향을 직접 선택. 입력한 폴더가 없으면 비교하지 않고, 출력 폴더는 입력 폴더와 같거나 그 안이면 거부하며 비어 있지 않으면 확인을 받음. 읽을 수 없는 파일은 자동 판정하지 않고 `읽을 수 없음`으로 표시. 한쪽에서 삭제된 폴더는 출력에 만들지 않음 |
| SY-01 SY-02 SY-03 SY-04 SY-05 | 동기화: 방식, 미리보기, 실행, 파일 작업, 안전장치 | ✅ | SY-04: 복사·삭제·이동·이름 변경·일시중지, 수정 시각·속성 맞춤 ✅. 스캔이 불완전하면 동기화를 막음 |
| SY-06 | 파일 시스템 메타데이터 동기화 | 🟡 | NTFS DACL 복사(옵션) ✅ — 권한 있는 환경에서의 실제 복사는 미검증. 8.3 별칭 ❌ |
| VF-01 | 로컬·네트워크 경로 | ✅ | 로컬·네트워크 드라이브·UNC |
| VF-02 | FTP 서버 비교 | 🟡 | 평문 FTP(WinInet) ✅. SFTP/FTPS ❌ |
| AR-01 AR-02 SN-01 | 압축 파일·스냅샷 비교 | ✅ | .zip, .bcss 읽기 전용 비교. AR-02: 폴더로 펼치거나 "하나의 파일로 비교(헥스)"를 선택 |
| VF-03~06 | SFTP/FTPS, 클라우드, Subversion, 휴대 장치 | ❌ | SSH/TLS 스택, OAuth·서명 키, svn 클라이언트, 휴대 장치가 없어 만들거나 검증할 수 없음 |
| AU-01~05 | 자동화: 세션·워크스페이스 저장, 명령줄, 스크립트 | ✅ | 스크립트 언어: set/echo/run/if/goto/exit |
| IN-01 IN-02 IN-03 | 통합: VCS 도구 연동, 탐색기 메뉴, 끌어놓기 | ✅ | IN-02는 레지스트리 동사 방식(IContextMenu 셸 확장 DLL은 아님) |
| IN-04 | 비교 전 변환 규칙 | ✅ | `[Convert] .확장자=명령줄`(`%1` 입력, `%2` 출력 텍스트) |
| IN-05 | 소스 제어 체크인/아웃 | ❌ | MSSCCI 제공자 DLL이 필요해 제외 |
| CU-01 CU-02 | 설정 대화상자, 관리자 정책 | ✅ | 글꼴·색·단축키·외부 도구 / 레지스트리 정책(읽기 코드만, HKLM 실제 차단은 미검증) |
| CU-03 | 새 버전 자동 확인 | ✅ | 도움말 → 업데이트 확인, `[Update] AutoCheck=1`이면 시작 시 확인. 기본 피드는 GitHub 릴리스 API(`[Update] Url`로 변경) |
| 비기능 | 성능·안정성·접근성 | 🟡 | 스레드 스캔·동기화, 삭제 전 확인(휴지통), 자동 백업, 유니코드, DPI ✅. 10만 파일 이상 규모는 미측정 |

## 측정한 성능 (이 개발 PC, 캐시 워밍 후)

| 시나리오 | 결과 |
| --- | --- |
| 폴더 비교: 파일 3만 개 + 폴더 300개, 양쪽 | 약 0.3초에 스캔, 메모리 약 18MB, 스캔 중에도 UI 응답 |
| 텍스트 비교: 2만 줄, 변경 150곳 | 약 0.4초 (Myers O(ND)) |
| 헥스 비교: 50MB 두 파일 | 약 0.6초, 메모리 약 130MB |
| 레지스트리 비교: HKCU\Software 전체(항목 40만 개 한도) | 약 9초(UI 스레드에서 실행), 메모리 약 170MB |

## 검증 방법

- `python tools/test_regressions.py`: 격리된 입력·설정 경로에서 전체 재빌드와 저장 오류 주입, 변경 상태·종료·스레드 취소, 명령줄 비교·보고서·동기화 회귀 검사 57개를 실행합니다.
- [2026-10-14 수정 결과](APP_FIXES_2026-10-14.md): 안전한 저장, 미저장 변경 보호, 작업 종료 대기, 자동화 종료 코드 수정.

- `tools/ui.ps1` 로 실제 창을 띄워 컨트롤 ID로 클릭·입력·키 입력을 보내고, 화면 캡처와 결과 파일을 비교했습니다 (`tools/t_*.ps1` 시나리오).
- 이번 단계에서 확인한 시나리오: 버전/레지스트리/미디어 비교(합성 MP3·FLAC 포함), 헥스 바꾸기 후 저장, 구문 강조, 수동 정렬, 텍스트 편집(바꾸기·저장·CRLF 보존), XLSX 표 비교(공유 문자열·한글·엔티티), 복합 키·날짜 허용 오차, 폴더 3-way 병합(자동 판정 + 실행 결과 파일 + 충돌 파일의 텍스트 병합 세션 열기), 설정 대화상자 저장, 외부 도구 실행, 스크립트(변수·run·if·goto·종료 코드), 탐색기 메뉴 등록/해제, 크기·날짜 필터, 파일별 HTML 보고서, 이동·이름 바꾸기.
- 자동화로 검증하지 못한 것: 파일 끌어놓기(IN-03), 관리자 정책(HKLM/HKCU `Policies` 키는 일반 권한으로 쓸 수 없어 읽기 코드만 작성), 실제 FTP 서버(테스트 서버로만 확인), 10만 파일 이상의 규모.

## 구조

```
gui/
  main.asm      진입점, 프레임 창, 탭(세션) 관리, 메뉴, 키 처리(설정 가능한 단축키), 세션·워크스페이스 INI, 명령줄
  home.asm      홈 화면(카드 + 최근 세션 목록), 설정/최근 목록
  util.asm      컨트롤 생성, 폰트/DPI(설정 반영), 테마(색 재정의), 파일 대화상자, 상태 표시줄
  pathbar.asm   모든 뷰가 쓰는 경로 막대·버튼·체크박스·다크 리스트 머리글
  engine.asm    가변 배열, 크기/시각 포맷, 와일드카드, 문자열 빌더, UTF-8 출력
  textlib.asm   텍스트 입출력(인코딩 감지), 줄 분리, 키 정규화, LCS/Myers diff
  rich.asm      RichEdit 헬퍼 (배경색/글자색)
  folder.asm + folder_ui.inc/_vfs.inc/ftp.inc/sync_dlg.inc/folder_rep.inc   폴더 비교·동기화·VFS·FTP·보고서
  folder_exp.inc / excl_dlg.inc / export.asm   Excel·Word 내보내기(무압축 ZIP 기록기), 제외 선택 대화상자·오른쪽 클릭 메뉴
  fmerge.asm                   폴더 병합 (3-way)
  text.asm   + text_ui.inc/_gutter.inc/_syn.inc/_align.inc/edit_ui.inc   텍스트 비교·구문 강조·수동 정렬·편집
  merge.asm  + merge_ui.inc    텍스트 병합 (diff3)
  table.asm  + xlsx.inc/table_date.inc   표 비교 (CSV/XLSX, 날짜 허용 오차)
  hex.asm    + hex_ui.inc/hex_repl.inc   헥스 비교·바꾸기
  pic.asm    + pic_ui.inc/ui2  이미지 비교
  props.asm  + props_ver.inc/props_reg.inc/props_media.inc   버전·레지스트리·미디어 비교
  inflate.asm / vtab.asm       DEFLATE·CRC32, ZIP/스냅샷 가상 폴더
  settings.asm                 설정 대화상자, 정책, 단축키, 외부 도구, 입력 상자
  script.asm / shell.asm       스크립트 언어, 탐색기 통합
  inc/          매크로(invoke), Win32 상수, 앱 정의
src/bc.asm      이전 단계의 콘솔 도구 (bc.exe)
tools/          UI 자동 검증 스크립트, 정적 검사기(hazard.py), 테스트 서버·생성기, 크래시 로거(crash_debug.asm.txt)
```

`inc/macros.inc`의 `invoke` 매크로로 Win64 호출 규약(인자 레지스터, 섀도 공간, 스택 정렬)을 처리합니다.
`tools/hazard.py`는 `invoke` 인자 평가 순서 때문에 생기는 레지스터 덮어쓰기와, 16바이트 배수가 아닌 스택 프레임을 정적으로 검사합니다.

## 알려진 한계

- 텍스트 비교는 LCS 표(최대 3천만 칸)를 쓰므로 서로 크게 다른 대용량 파일은 Myers 알고리즘으로 넘어갑니다.
- 구문 강조는 단순 토크나이저입니다(문맥 의존 문법·여러 줄 문자열 일부 미지원). RichEdit 서식 지정이 느려 한 창당 약 6만 토큰까지만 칠합니다.
- 레지스트리 비교는 UI 스레드에서 동기 실행되고 항목 수 상한이 40만 개입니다.
- 헥스 비교는 파일 전체를 메모리에 올립니다.
- 다크 모드에서 탭 컨트롤 머리글은 시스템 테마(밝은색)를 따릅니다.
- 아이콘 버튼(Segoe MDL2 글리프 + 툴팁)은 폴더 비교·텍스트 비교·헥스·이미지·텍스트 병합·폴더 병합 보기의 도구 모음에 적용했습니다(표·버전·레지스트리·미디어·편집 보기는 동작 버튼이 적거나 없어 텍스트 버튼 그대로). 도구 모음은 창이 좁아지면 다음 줄로 넘어갑니다(폴더·텍스트·헥스·표·병합·이미지·폴더 병합).
- SFTP/FTPS, 클라우드, Subversion, MTP, MSSCCI, 자동 업데이트, macOS/Linux 포팅은 이 환경에서 구현·검증할 수 없어 제외했습니다.

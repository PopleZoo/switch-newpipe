# Handoff

## 최근 수정 (2026-07-29)

GitHub issue #3 / #4 대응. 둘 다 UI 레이어 버그다.

- issue #3 `검색 탭에서 X를 눌러도 키보드가 안 뜬다`
  - borealis는 포커스된 view에서 부모로 올라가며 action을 찾는다. 탭 본문은
    사이드바 항목의 부모가 아니라서, 사이드바에 포커스가 있으면 탭 본문에
    등록한 action은 절대 실행되지 않는다.
  - 검색 탭은 결과가 없을 때 focusable한 자식이 하나도 없다. 그래서 사이드바에서
    `A`를 눌러도 포커스가 본문으로 들어가지 못하고, `X` 검색 action에 도달할
    경로 자체가 없었다.
  - 홈 / 검색 / 구독 / 라이브러리 탭을 `AttachedView` 기반으로 바꾸고,
    탭 액션을 `registerTabAction`으로 등록해 사이드바 항목에도 같이 달았다.
  - 라이브러리 `RB` 비우기와 설정 `X` 기본값 복원은 파괴적 동작이라 일부러
    본문 전용 `registerAction`으로 남겼다. 두 탭 모두 본문에 focusable한 항목이
    있어서 도달 불가 문제도 없다.
- issue #4 `구독 탭이 거의 빈 화면이다`
  - borealis `Label`의 `lineHeight`는 px가 아니라 fontSize 배수다.
    `lineHeight="28"`에 `fontSize="18"`이면 한 줄이 약 504px이 된다.
  - 두 줄짜리 본문 라벨이 화면 전체를 먹어서 카드 그리드가 화면 밖으로
    밀려나 있었다. 사용자가 본 `빈 상자`가 이 라벨이다.
  - `resources/xml/tabs/subscriptions.xml`, `resources/xml/tabs/library.xml`,
    `resources/xml/activity/stream_detail.xml`의 값을 `1.55`로 고쳤다.
- 위 두 이슈를 파다가 같이 나온 포커스 소실 버그
  - borealis는 포커스된 view가 삭제되면 `currentFocus`를 nullptr로 지우고,
    `Application::navigate`는 포커스가 없으면 그냥 return한다. 즉 포커스를
    잃으면 방향키 이동이 완전히 죽는다.
  - 그리드 재생성 전에 있던 기존 회피 코드는 실제로 동작하지 않았다.
    `scrollFrame->getDefaultFocus()`가 `lastFocusedView`를 따라가서 곧 삭제될
    그 카드를 다시 돌려주기 때문에 `giveFocus`가 no-op이 된다. 결과적으로
    카드에 포커스를 둔 채 `X` 새로고침을 하면 UI가 먹통이 됐다.
  - `include/view/tab_focus.hpp`의 `newpipe::release_grid_focus`로 통일했다.
    항상 focusable한 사이드바 항목으로 포커스를 옮긴 뒤 그리드를 비운다.
  - `AutoSidebarItem`의 `A` 액션도 같은 문제였다. 비어 있는 탭에서 `A`를 누르면
    `giveFocus(nullptr)`이 되어 포커스가 사라졌다. 탭에 focusable한 항목이
    있을 때만 포커스를 넘기도록 고쳤다.
- 검증
  - 호스트 `g++ -fsyntax-only`로 수정한 translation unit 전부 확인
  - 실기 `.nro` 빌드와 실기 동작은 아직 확인하지 않았다
  - 실기 체크 항목은 `docs/testing.md`의 issue #3 / #4 절 참고

## 알려진 관련 제한

`vendor/borealis`의 `SwitchImeManager::openForText`는 결과 버퍼가 `char[0x100]`로
고정되어 있다. 구독 탭 세션 대화상자의 쿠키 직접 입력은 `maxStringLength`를
4096으로 넘기지만 실제로는 255자에서 잘린다. 쿠키 문자열은 거의 항상 그보다
길기 때문에 이 경로는 사실상 동작하지 않는다고 봐야 한다. 파일 import 경로는
영향이 없다. 고치려면 vendor 쪽 버퍼를 `maxStringLength` 기준 heap 버퍼로
바꿔야 한다.

## 현재 상태 (2026-04-06)

`Switch-NewPipe`는 이제 단순 스캐폴드가 아니라, 실제 YouTube 데이터 탐색과 실기 재생 루프까지 연결된 Switch MVP다.

## 지금까지 구현한 것

- Borealis 기반 메인 앱과 탭 UI
  - 홈
  - 검색
  - 구독
  - 라이브러리
  - 설정
- 실제 YouTube 데이터 계층
  - `youtubei/v1/search` 기반 검색
  - 홈 카테고리 `추천 / 라이브 / 음악 / 게임`
  - 로그인 세션이 있으면 `FEwhat_to_watch` 기반 개인화 추천 홈 우선 시도
  - `youtubei/v1/browse` + `FEsubscriptions` 기반 구독 피드
  - player API 기반 상세 설명 / 채널 ID 보강
  - watch-next 기반 연관 영상 파서
  - `youtubei/v1/browse` 기반 재생목록 파서
  - watch page continuation + `youtubei/v1/next` 기반 댓글 파서
  - channel RSS 기반 채널 업로드 피드
- 인증 계층
  - 외부 쿠키 import
  - 세션 저장
  - `SAPISIDHASH` 생성
- 라이브러리 계층
  - 최근 시청 저장
  - 즐겨찾기 저장
- 설정 계층
  - 시작 탭 저장
  - 기본 홈 카테고리 저장
  - 재생 품질 정책 저장
  - 짧은 영상 숨기기 저장
- 상세 / 채널 / 연관 추천 activity
- 상세 extras 메뉴에서 재생목록 / 댓글 activity 진입
- 썸네일 비동기 로더
- Switch 전용 SDL2/OpenGL ES/mpv 플레이어
- Borealis 종료 -> 플레이어 실행 -> 종료 후 Borealis 복귀 루프
- 로딩 progress circle + 상태 문구
- `sdmc:/switch/switch_newpipe.log` 로그 저장
- `make host` 공용 검증 경로

## 저장 파일

- 로그: `sdmc:/switch/switch_newpipe.log`
- 설정: `sdmc:/switch/switch_newpipe_settings.json`
- 라이브러리: `sdmc:/switch/switch_newpipe_library.json`
- 인증 import 기본 경로: `sdmc:/switch/switch_newpipe_auth.txt`
- 로그인 세션: `sdmc:/switch/switch_newpipe_session.json`

## 현재 입력 흐름

- 목록
  - `A`: 바로 재생
  - `Y`: 상세
- 상세
  - `A`: 재생
  - `X`: 채널 업로드
  - `Y`: 연관 추천
  - `LB`: 재생목록 / 댓글 메뉴
  - `RB`: 즐겨찾기 토글
- 플레이어
  - `A`: 일시정지 / 재개
  - `B`: 종료
  - `위 / 아래`: 볼륨

좌우 seek는 아직 비활성화 상태다.

## 재생 전략

- `표준 720p`
  - 720p HLS 우선
  - 실패 시 progressive fallback
- `호환성 우선`
  - progressive MP4 우선
- `데이터 절약`
  - 480p 부근 progressive 우선

이 정책은 설정 탭과 `switch_newpipe_settings.json`에 저장된다.

## 실기에서 이미 확인된 것

- 앱 부팅 성공
- 홈 / 검색 / 상세 진입 성공
- 썸네일 로딩 성공
- 실제 YouTube 영상 재생 성공
- 플레이어 `A / B / 위 / 아래` 입력 성공
- 로딩 progress circle + 상태 문구 표시
- 로그 파일 생성 및 FTP 회수 가능

## 이번 라운드에서 추가된 것

- watch-next 기반 연관 영상 파서
- 재생목록 파서 추가
- 댓글 파서 추가
- 로그인 기반 추천 홈 1차 연결
- 설정 탭 추가
- 설정 저장 추가
- 시작 탭 적용
- 기본 홈 카테고리 적용
- 재생 품질 정책 3종 적용
- 짧은 영상 숨기기 필터 적용
- 실기 테스트 체크리스트 문서 추가

## 호스트에서 검증한 것

- `make host`
- `./build/host/switch_newpipe_host --related 'https://www.youtube.com/watch?v=dQw4w9WgXcQ'`
  - watch-next 기반 연관 목록 12개 확인
- `./build/host/switch_newpipe_host --resolve 'https://www.youtube.com/watch?v=dQw4w9WgXcQ'`
  - 720p HLS 해석 성공 확인
- `./build/host/switch_newpipe_host --playlist 'https://www.youtube.com/watch?v=ZZcuSBouhVA&list=PL8F6B0753B2CCA128'`
  - 재생목록 항목 48개 확인
- `./build/host/switch_newpipe_host --comments 'https://www.youtube.com/watch?v=dQw4w9WgXcQ'`
  - 댓글 19개 파싱 확인
- `./build.sh`
  - 최신 Switch `.nro` 빌드 성공

## 아직 실기에서 확인 못 한 것

- 설정 탭 UI 동작
- 설정 persistence
- 재생 품질 3종 전환
- watch-next 연관 추천 UI 동작
- 실제 계정 쿠키 기반 구독 피드
- 로그인 기반 추천 홈
- 재생목록 / 댓글 UI 동작

## 현재 남아 있는 제한

- 로그인은 외부 쿠키 import 방식이라 앱 내부 OAuth / WebView는 없다
- 채널 피드는 아직 RSS 기반이다
- 다운로드는 아직 없다
- 좌우 seek는 비활성화 상태다
- 화질 수동 선택 UI가 없다
- 댓글 / 재생목록은 1페이지 파서만 있다

## 주요 파일

1. `src/common/youtube_catalog_service.cpp`
   - 실제 YouTube 검색 / 홈 / 구독 / 채널 / 연관 데이터 수집
2. `src/common/auth_store.cpp`
   - 쿠키 import, 세션 저장, SAPISIDHASH 생성
3. `src/common/library_store.cpp`
   - 최근 시청 / 즐겨찾기 저장
4. `src/common/settings_store.cpp`
   - 시작 탭 / 홈 카테고리 / 재생 품질 / 짧은 영상 필터 저장
5. `src/common/youtube_resolver.cpp`
   - 설정 기반 재생 해석 전략
6. `src/activity/stream_detail_activity.cpp`
   - 상세 / 채널 / 연관 진입
7. `src/switch/switch_player.cpp`
   - Switch 전용 플레이어
8. `src/main.cpp`
   - Borealis UI와 플레이어 런타임 루프 연결
9. `src/common/log.cpp`
   - `switch_newpipe.log` 저장

## 다음 우선순위

1. 설정 탭과 설정 persistence 실기 검증
2. 재생 품질 3종 실기 검증
3. 로그인 import + 구독 피드 실기 검증
4. 홈 개인화 추천 / 구독 실기 검증
5. 채널 browse/parser 확장
6. seek / OSD / 화질 선택 UI 복구
7. 다운로드 / 백그라운드 재생 검토

## 참고

- `reference/NewPipe`: 기능 우선순위 기준
- `reference/` 아래의 무시된 Switch 재생 레퍼런스 앱: Borealis + mpv/ffmpeg 재생 구조 참고
- `reference/wiliwili`: UI/플레이어 구조 참고
- 실제 빌드 의존성은 `vendor/borealis`, `vendor/lunasvg`, `vendor/third_party`, `vendor/switch-portlibs`에 직접 vendoring 되어 있음
- `docs/testing.md`: 실기 테스트 체크리스트

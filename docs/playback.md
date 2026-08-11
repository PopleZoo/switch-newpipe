# Playback

## 현재 상태

재생 코드는 이미 연결되어 있다. 현재 앱은 Borealis UI에서 재생 요청을 큐에 넣고, UI를 빠져나온 뒤 Switch 전용 SDL2/mpv 플레이어를 실행한다.

## 현재 재생 구조

1. 목록에서 `A`로 영상 선택
2. `PlaybackRequest` 생성
3. Borealis 종료
4. `SwitchPlayer` 실행
5. YouTube URL이면 `YouTubeResolver`로 playable stream 해석
6. mpv 재생
7. `B`로 종료 후 Borealis 재시작

## 입력

- 목록 화면
  - `A`: 바로 재생
  - `Y`: 상세 정보
- 플레이어 화면
  - `A`: 일시정지 / 재개
  - `B`: 종료
  - `위 / 아래`: 볼륨
  - `좌 / 우`: 10초 이동
  - `LB / RB`: 60초 이동
  - `X`: 화질 메뉴
  - `Y`: OSD 고정 표시 토글
  - 화질 메뉴가 열린 동안: `위 / 아래` 선택, `A` 적용, `B / X / Y` 닫기

## 현재 해석 전략

### 1. 일반 direct URL

- 바로 재생
- 필요 시 캐시 브리지 사용

### 2. YouTube URL

- Android `youtubei/v1/player`로 기본 playable format 조회
- 720p adaptive가 있으면 iOS HLS manifest 확보 시도
- 현재 최신 코드 기준 우선순위
  1. 원본 720p HLS manifest direct 재생 (throttle 면역, mpv가 세그먼트 단위 fetch)
  2. HLS가 없으면 tokenless Android VR 720p AVC/AAC direct URL을 UMP로 재생
  3. Android VR/UMP가 실패하면 progressive `itag18`(`ratebypass=yes`, 360p)
  - `hls-bitrate=max` 적용
  - 첫 프레임 전 실패 시 fallback stream으로 자동 전환

> **YouTube 스트림 차단 주의 (2026-06 이후):** `ratebypass=yes`가 없는
> `c=ANDROID` googlevideo `videoplayback` URL(=720p adaptive)은 초기 버스트
> (~6-13MB, itag136 기준 약 1분 분량)만 주고 그 뒤 **모든 요청을 HTTP 403으로
> 하드 차단**한다. 그래서 단일 GET이든 `&range=` 청크든 버스트 이후를 못 받고
> **~1:04에서 멈춘다** (issue #5). 현재는 일반 GET 대신 tokenless `ANDROID_VR`
> URL을 UMP POST로 1MiB씩 받고 envelope의 MEDIA만 캐시에 써서 이 제한을 우회한다.

### 3. Tokenless Android VR UMP adaptive stream

- `/sw.js_data`의 서버 발급 WEB visitorData를 Android VR player 요청에 사용한다.
- player 요청에 `serviceIntegrityDimensions.poToken`을 넣지 않고, video/audio URL에도
  `pot` query parameter를 추가하지 않는다.
- 영상과 오디오를 각각 1MiB UMP range POST로 받고 type 21 MEDIA를 캐시에 기록한다.
- UMP redirect와 StreamProtectionStatus를 검사한다.
- WebApplet을 사용하지 않으므로 브라우저 제한이나 title whitelist의 영향을 받지 않는다.

### 4. Progressive stream

- `switchcache://` 커스텀 프로토콜 사용
- 백그라운드 다운로드 + mpv 읽기 브리지
- 긴 영상도 전체 다운로드 완료까지 기다리지 않고 재생 시작 가능
- `ratebypass=yes`가 있으면 단일 GET으로 직접 다운로드 (면역, 끝까지 재생됨)
- `ratebypass`가 없는 일반 Android googlevideo GET은 신뢰하지 않는다. 720p는 위 UMP
  경로를 사용하고, 실패하면 `ratebypass=yes` progressive로 전환한다.

## 로딩 UI

재생 준비 중에는 검은 화면 대신 progress circle과 상태 문구를 표시한다.

예시 상태:

- `RESOLVING YOUTUBE STREAM`
- `REQUESTING 720P HLS STREAM`
- `REQUESTING 720P UMP STREAM`
- `OPENING MEDIA STREAM`
- `DOWNLOADING VIDEO DATA`
- `BUFFERING FIRST FRAME`

## 재생 OSD

재생이 시작된 뒤에는 입력 반응형 OSD를 영상 위에 직접 그린다.

- 자동 표시 시점
  - 첫 재생 시작 직후
  - `A` pause / resume
  - `위 / 아래` 볼륨 변경
  - `좌 / 우`, `LB / RB` 시간 이동
- 표시 정보
  - 제목
  - 재생 상태
  - 현재 품질 라벨
  - 진행 바 / 경과 시간 / 총 길이
  - 진행 바 위의 버퍼 구간 (지금 이동 가능한 범위)
  - 볼륨 바
- `X / Y`를 누르면 OSD를 잠깐 띄우는 것이 아니라 고정 표시로 토글된다

## 시간 이동 (seek)

`좌 / 우` 10초, `LB / RB` 60초다. 경로마다 이동 가능한 범위가 다르다.

### HLS / manifest 경로

mpv가 세그먼트를 직접 가져오므로 전체 구간 이동이 된다. 브리지를 쓰지 않으니
별도 제한도 걸지 않는다.

### 스트림 브리지 경로 (progressive / UMP)

브리지는 다운로드를 캐시 파일에 순차적으로 쌓는다. `switchcache` stream callback의
`seek_fn`은 이미 받아둔 영역(초기 prefix `[0, retained_edge]` 또는 현재 다운로드
영역 `[restart_base, streamed_bytes]`) 안의 offset은 캐시에서 즉시 서빙한다.

버퍼 밖으로 이동하면 seek을 거절하지 않고 다운로더를 해당 byte offset에서
**재시작**한다:

- `stream_seek`가 이동 지점을 `stream_seek_restart_` / `audio_seek_restart_` 플래그와
  타깃으로 기록하고 `stream_cv_` / `audio_cv_`를 알린다.
- 영상 다운로드 루프(`perform_chunked_ranged_download`)와 오디오 prefetch 루프
  (UMP / ranged 양쪽)는 다음 반복에서 플래그를 소비해 `streamed_bytes_` /
  `audio_downloaded_bytes_`를 타깃으로 되돌리고 그 위치부터 다시 받는다.
- fragmented MP4에서 mov demuxer가 sidx로 계산한 moof offset으로 seek하므로
  재시작 지점은 항상 fragment 경계이고, 바이트가 도착하면 깨끗하게 재생이 재개된다.
- 앞으로 이동이 버퍼 끝에 막히면 막지 않는다. 이동을 수락하고 다운로더를 재시작한 뒤
  재생이 따라잡으면 mpv가 스스로 버퍼링 상태를 표시한다.
- 분리된 오디오 트랙이 있으면 video / audio 양쪽 다운로더를 모두 재시작한다.
- 라이브는 이동하지 않는다.

`size_fn`은 총 크기를 알려준다. 크기를 모르면 ffmpeg의 mov demuxer가 fragmented
MP4를 열기 전에 fragment index 전체를 훑으므로(slow 스트림에서 "파일 전체를 받고
재생"이 된다) 열기까지 걸린다. 크기를 주면 demuxer는 열 때 파일 끝으로 한 번
이동한다(mfra 확인 / moov-at-end). 이 때 seek이 버퍼 밖이므로 앞쪽 점프
(`stream_jump_*`)가 동작한다:

- 앞쪽 점프: 다운로더가 점프 지점(파일 끝)에서 한 chunk를 내려받은 뒤, 저장해둔
  기존 frontier로 돌아와 순차 다운로드를 이어간다. 점프에 생긴 갭은 순차 다운로드가
  채운다.
- `stream_read`는 `[0, retained_edge]` prefix와 `[restart_base, streamed_bytes]`
  활성 영역만 원본으로 서빙하고, 사이 갭은 채워질 때까지 대기한다. 잘못된 바이트를
  넘기지 않는다.
- EOF를 넘어가는 seek은 점프하지 않고 읽기에서 EOF로 처리한다.

> **회귀 시 확인 순서:** 재생 자체가 깨지면 `stream_open`의 `seek_fn`을 다시
> `nullptr`로 돌려 seek만 끄면 이전 동작으로 복귀한다. 720p UMP 경로가 실기에서
> 검증된 경로라 이 순서를 지킬 것.

## 화질 선택

- `X`를 누르면 현재 영상에서 실제 재생 가능한 해상도 목록을 중앙 패널로 띄운다.
  목록은 resolver가 수집한다. `ResolvedPlayback::available_heights`:
  - progressive `video/mp4` format 높이
  - adaptive `video/mp4` + `avc1` video-only format 높이
  - iOS HLS master manifest의 variant 높이
  - WebM/VP9은 데코더가 없어 목록에서 제외된다
- `A`로 선택하면 해당 높이로 `resolve_with_height()`로 재해석한 뒤 현재 재생
  위치를 유지한 채 스트림/오디오 브리지를 재구성하고 mpv를 다시 로드한다.
- `AUTO`는 휴대 모드 720p, 독 모드(`AppletOperationMode_Console`) 1080p 기준으로
  해석한다.
- 라이브 스트림은 화질 선택이 불가능하다.
- 실패하면 이전 화질로 자동 복구하고, 복구도 실패하면 플레이어를 종료한다.

## 이어보기 (Continue Watching)

- 플레이어는 10초마다, 일시정지 시, 종료 시에 현재 위치를
  `LibraryStore::update_history_position(video_id, pos, dur)`로
  `sdmc:/switch/switch_newpipe_library.json`에 저장한다 (최근 시청 기록의
  `position_seconds` / `duration_seconds` 필드).
- 영상을 다시 열면 `build_playback_request`가 저장된 위치를 찾아
  `PlaybackRequest::start_position_seconds`로 전달한다.
- 플레이어는 파일 로드 후 해당 위치로 seek한다. 브리지 경로면 버퍼 밖 위치라도
  위 재시작 메커니즘으로 그 지점부터 받아서 재생한다.

## 로그

- Switch: `sdmc:/switch/switch_newpipe.log`
- FTP 예시: `ftp://192.168.1.16:5000/switch/switch_newpipe.log`

## 현재 제한

- 브리지 경로의 seek은 캐시 안 영역은 즉시, 밖은 다운로더 재시작으로 처리된다 (실기 검증 대기)
- 화질 선택 / 이어보기 구현 완료, 실기 검증 대기
- tokenless Android VR UMP는 실기에서 71,936,808 B video와 8,565,660 B audio 전체 완료
- 라이브/장시간 스트림의 예외 처리 강화가 더 필요
- 플레이어 OSD는 들어갔지만 실기 UI 튜닝은 아직 필요하다

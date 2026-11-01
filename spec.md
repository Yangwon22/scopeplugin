# SPEC-001: ScopePlugin — VP Shot Save Tool

## 요약
UE5 5.7 에디터 플러그인으로 VP 세션의 카메라/씬 파라미터를 수집하여 JSON으로 HTTP POST 전송한다.

## 배경
VP(Virtual Production) 촬영 현장에서 매 Shot마다 카메라, nDisplay, ICVFX 설정값을 기록해야 한다.
UMetaData가 UE 5.5+에서 제거되어 GConfig INI 기반으로 설정을 저장한다.
에디터 패널 UI에서 Shot/Scene/Take/Status를 입력하고 Save 버튼으로 전송한다.

## 요구사항
- [ ] 에디터 패널: SessionID, Shot/Scene/Take, Status(OK/NG/CIRCLE), Save 버튼
- [ ] 필터 체크박스: Camera / Tracking / ICVFX / nDisplay / Scene (GConfig INI 저장)
- [ ] ACineCameraActor: 위치, 회전, FOV, Aperture, FocusDistance, FocalLength, 센서 크기
- [ ] Live Link: 연결 상태, Subject 이름, 트래킹 위치/회전
- [ ] APostProcessVolume: Bloom, Vignette, AutoExposureBias
- [ ] Lights: SkyLight 강도, DirectionalLight 강도/회전
- [ ] nDisplay: ConfigName, 해상도, Genlock, FPS (리플렉션 기반 — 하드 의존 없음)
- [ ] ICVFX: InnerFrustum 비율, Overscan, SoftEdge, LightCard UV/밝기/색온도, Chromakey 여부
- [ ] UWorld::GetMapName(), FApp::GetFixedDeltaTime() 수집
- [ ] 스크린샷: InnerFrustum 뷰 / NDC 뷰 캡처 후 파일 저장
- [ ] JSON 직렬화 → HTTP POST http://localhost:3000/api/shot-save

## 태스크 목록
| ID | 작업 | 담당 에이전트 | 예상 파일 수 |
|----|------|-------------|------------|
| T1 | uplugin + Build.cs + 타입/헤더 정의 | executor | 7 |
| T2 | 모듈 진입점 + 패널 UI (메인/필터/저장) | executor | 4 |
| T3 | 씬 데이터 수집기 (Camera/Scene/ICVFX) | executor | 3 |
| T4 | HTTP 송신 + 스크린샷 캡처 | executor | 2 |

## 수락 기준 (Acceptance Criteria)
- Given: 에디터가 열린 상태 / 씬에 CineCameraActor 존재
  When: 패널에서 Shot 정보 입력 후 Save 클릭
  Then: JSON이 http://localhost:3000/api/shot-save 로 POST 전송됨

- Given: 필터 체크박스에서 Camera 체크 해제
  When: Save 클릭
  Then: JSON payload에 "camera" 키 없음

- Given: nDisplay 미설치 환경
  When: 플러그인 로드
  Then: 빌드/런타임 오류 없이 동작 (리플렉션 기반)

## 기술 결정 사항
- 언어: C++17, UE5 5.7
- 패턴: IModuleInterface + Nomad Tab + SCompoundWidget (Slate)
- nDisplay 접근: TFieldIterator 리플렉션 (DisplayCluster 하드 링크 없음)
- 설정 저장: GConfig INI (ProjectSaved/ScopePlugin/)
- 파일 크기: 최대 300줄, 목표 200줄
- HTTP: FHttpModule, JSON: TSharedPtr<FJsonObject>

## 리스크
- Live Link API: UE5.7에서 EvaluateFrame_AnyThread 시그니처 변경 가능 → GetSubjects로 대체
- nDisplay 리플렉션: 버전별 프로퍼티명 변경 가능 → 로그 출력 후 빈 값으로 처리
- 스크린샷 타이밍: 비동기 캡처 → 파일명만 반환하고 실제 저장은 다음 프레임

## 변경 내용


## 검증

- [ ] `python tools/test_regressions.py` 통과
- [ ] `gui/`를 고쳤다면 `tools/test_*`에 그 동작을 확인하는 검사를 추가하거나 고쳤습니다
  - 화면 동작(색, 강조, 키): `tools/test_ui.c`
  - 저장·서식 함수: `tools/test_safe_save.c`
  - 변경 상태·종료·작업 스레드: `tools/test_view_state.c`
  - 명령줄·보고서·내보내기: `tools/test_regressions.py`

검사를 추가할 수 없으면 아래 줄에 이유를 적습니다. 이 줄이 비어 있으면 CI가 실패합니다.

테스트 생략 사유:

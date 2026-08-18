import solver


def board_from(rows):
    return tuple(v for row in rows for v in row)


def test_merge_shifts_and_combines_pairs():
    start = board_from([[2, 2, 4, 4], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]])
    moved, score = solver.apply_move(start, "left")
    assert moved[:4] == (4, 8, 0, 0)
    assert score == 12


def test_merge_happens_once_per_move():
    # 2 2 2 2 -> 4 4, а не 8: слитая плитка не участвует в новом слиянии.
    start = board_from([[2, 2, 2, 2], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]])
    moved, _ = solver.apply_move(start, "left")
    assert moved[:4] == (4, 4, 0, 0)


def test_move_into_wall_is_not_legal():
    start = board_from([[2, 4, 8, 16], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]])
    assert solver.legal_moves(start) == ["down"]


def test_full_board_without_pairs_has_no_move():
    start = board_from([[2, 4, 2, 4], [4, 2, 4, 2], [2, 4, 2, 4], [4, 2, 4, 2]])
    assert solver.best_move(start) is None


def test_solver_takes_the_only_available_move():
    start = board_from([[2, 4, 8, 16], [0, 0, 0, 0], [0, 0, 0, 0], [0, 0, 0, 0]])
    assert solver.best_move(start, depth=2) == "down"

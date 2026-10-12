import gc
from pybricks.tools import multitask, run_task, wait

closed = []


async def worker(name, fail=False):
    try:
        while True:
            await wait(0)
    finally:
        gc.collect()
        closed.append(name)
        if fail:
            raise ValueError(name)


async def broken():
    await wait(0)
    raise RuntimeError("original")


try:
    run_task(multitask(worker("first", True), worker("second", True), broken()))
except RuntimeError as error:
    assert str(error) == "original"
else:
    assert False
assert closed == ["first", "second"]
assert not run_task()

# Explicit close without an operation error reports the first cleanup error,
# closes every task, and is idempotent even when cleanup failed.
closed.clear()


async def manual():
    group = multitask(worker("first", True), worker("second", True))
    next(group)
    try:
        group.close()
    except ValueError as error:
        assert str(error) == "first"
    else:
        assert False
    assert closed == ["first", "second"]
    group.close()
    assert closed == ["first", "second"]


run_task(manual())
# run_task also closes an iterator when its own loop handles the failure.
closed.clear()


class BrokenIterator:
    def __iter__(self):
        return self

    def __next__(self):
        raise RuntimeError("iteration")

    def close(self):
        assert run_task()
        closed.append("iterator")
        raise ValueError("cleanup")


try:
    run_task(BrokenIterator())
except RuntimeError as error:
    assert str(error) == "iteration"
else:
    assert False
assert closed == ["iterator"]
assert not run_task()
print("cleanup passed")

# SPDX-License-Identifier: MIT
# Float-valued binding arguments retain truncation and reject invalid integers.
from pybricks.parameters import Color

assert tuple(Color(1.9)) == (1, 100, 100)
assert tuple(Color(-1.9)) == (359, 100, 100)
assert tuple(Color(360.9)) == (0, 100, 100)
for value, error in ((float("nan"), ValueError), (float("inf"), OverflowError),
                     (-float("inf"), OverflowError), (1e30, OverflowError)):
    for args in ((value, 100, 100), (0, value, 100), (0, 100, value)):
        try:
            Color(*args)
        except error:
            pass
        else:
            raise AssertionError("invalid float accepted")
print("float validation passed")

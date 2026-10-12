# SPDX-License-Identifier: MIT
from pybricks.tools import Matrix

# Representable binary fractions make the expected decimal values independent
# of the printer's rounding rule. Include gravity in mm/s^2 and large values.
assert tuple(Matrix([[1, 2, 3], [4, 5, 6]]).T) == (1, 4, 2, 5, 3, 6)
assert tuple(Matrix([[1], [2], [3]]).T) == (1, 2, 3)
a = Matrix([[9806.625, -1234.5, 0], [12345678, -0.125, 1.25]])
for matrix in (a, a.T, -a, a * 2):
    restored = eval(repr(matrix))
    assert restored.shape == matrix.shape
    assert tuple(restored) == tuple(matrix)
assert "9806.625" in repr(a)

text = repr(Matrix([[float("inf"), -float("inf"), float("nan")]]))
assert "inf" in text and "-inf" in text and "nan" in text
large = Matrix([[1e30, -1e30]])
restored = eval(repr(large))
for i in range(2):
    assert abs(restored[0, i] / large[0, i] - 1) < 0.001
print("matrix print passed")

# IMU regression checks

Run `make -C tests/imu check` from the repository root. The host executable uses
production PBIO IMU and geometry code with synthetic driver callbacks, clock,
motor state and storage requests. It checks read-only samples, current-step
orientation, calibration rejection without partial mutation or storage writes,
threshold clipping and rejection of non-finite directions. No board timing or
physical calibration is measured.

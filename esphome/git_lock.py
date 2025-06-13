"""File locking for git operations to prevent race conditions."""

from contextlib import contextmanager
import hashlib
import logging
from pathlib import Path
import sys
import tempfile
import time

# Platform-specific imports
if sys.platform == "win32":
    import msvcrt
else:
    import fcntl

_LOGGER = logging.getLogger(__name__)

# Global lock directory
LOCK_DIR = Path(tempfile.gettempdir()) / "esphome_git_locks"
LOCK_DIR.mkdir(exist_ok=True)


def _acquire_lock_unix(lock_file, timeout, identifier):
    """Acquire lock on Unix systems using fcntl."""
    start_time = time.time()
    last_log_time = start_time
    while True:
        try:
            fcntl.flock(lock_file.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
            return True
        except OSError:
            elapsed = time.time() - start_time
            if elapsed > timeout:
                raise TimeoutError(
                    f"Could not acquire lock for {identifier} within {timeout}s"
                )

            # Log progress every 10 seconds
            if time.time() - last_log_time > 10:
                _LOGGER.info(
                    f"Still waiting for lock {identifier} ({elapsed:.1f}s elapsed)..."
                )
                last_log_time = time.time()

            time.sleep(0.1)


def _release_lock_unix(lock_file):
    """Release lock on Unix systems."""
    try:
        fcntl.flock(lock_file.fileno(), fcntl.LOCK_UN)
    except Exception:
        pass


def _acquire_lock_windows(lock_file, timeout, identifier):
    """Acquire lock on Windows systems using msvcrt."""
    start_time = time.time()
    while True:
        try:
            msvcrt.locking(lock_file.fileno(), msvcrt.LK_NBLCK, 1)
            return True
        except OSError:
            if time.time() - start_time > timeout:
                raise TimeoutError(
                    f"Could not acquire lock for {identifier} within {timeout}s"
                )
            time.sleep(0.1)


def _release_lock_windows(lock_file):
    """Release lock on Windows systems."""
    try:
        msvcrt.locking(lock_file.fileno(), msvcrt.LK_UNLCK, 1)
    except Exception:
        pass


@contextmanager
def git_operation_lock(identifier: str, timeout: float = 30.0):
    """
    Acquire a file lock for a git operation.

    :param identifier: Unique identifier for the operation (e.g., repo URL or path)
    :param timeout: Maximum time to wait for the lock in seconds
    """
    # Create a safe filename from the identifier
    lock_name = hashlib.sha256(identifier.encode()).hexdigest()[:16]
    lock_path = LOCK_DIR / f"{lock_name}.lock"

    # Ensure lock file exists
    lock_path.touch(exist_ok=True)

    lock_file = None
    acquired = False

    try:
        # Open in binary mode for Windows compatibility
        lock_file = open(lock_path, "r+b")

        # Platform-specific lock acquisition
        if sys.platform == "win32":
            acquired = _acquire_lock_windows(lock_file, timeout, identifier)
        else:
            acquired = _acquire_lock_unix(lock_file, timeout, identifier)

        if acquired:
            _LOGGER.debug(f"Acquired lock for {identifier}")

        yield

    finally:
        if lock_file:
            if acquired:
                # Platform-specific lock release
                if sys.platform == "win32":
                    _release_lock_windows(lock_file)
                else:
                    _release_lock_unix(lock_file)
                _LOGGER.debug(f"Released lock for {identifier}")
            lock_file.close()


@contextmanager
def platformio_init_lock(timeout: float = 30.0):
    """Lock for PlatformIO initialization to prevent race conditions."""
    with git_operation_lock("platformio_init", timeout=timeout):
        yield


@contextmanager
def platformio_install_lock(package_name: str, timeout: float = 300.0):
    """Lock for PlatformIO package installation to prevent race conditions."""
    _LOGGER.info(
        f"Waiting for PlatformIO package installation lock ({package_name})..."
    )
    with git_operation_lock(f"platformio_install_{package_name}", timeout=timeout):
        _LOGGER.info(f"Acquired PlatformIO package installation lock ({package_name})")
        yield
        _LOGGER.info(f"Released PlatformIO package installation lock ({package_name})")

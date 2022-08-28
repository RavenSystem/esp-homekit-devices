#!/usr/bin/env python3
import sys
import argparse
import subprocess
import os
import serial
import threading
import re
import time


SHORT_OUTPUT_TIMEOUT = 0.25  # timeout for resetting and/or waiting for more lines of output
TESTCASE_TIMEOUT = 60
TESTRUNNER_BANNER = "esp-open-rtos test runner."
RESET_RETRIES = 10  # retries to receive test runner banner after reset


def run(env_a, env_b, cases):
    counts = dict((status, 0) for status in TestResult.STATUS_NAMES.keys())
    failures = False
    for test in cases:
        if test.case_type == 'dual':
            if env_b is None:
                res = TestResult(TestResult.SKIPPED, 'Dual test case skipped')
            else:
                res = test.run(env_a, env_b)
        else:
            res = test.run(env_a)
        counts[res.status] += 1
        failures = failures or res.is_failure()

    print("%20s: %d" % ("Total tests", sum(c for c in counts.values())))
    print()
    # print status counts for tests
    for c in sorted(counts.keys()):
        print("%20s: %d" % (TestResult.STATUS_NAMES[c], counts[c]))

    return failures == 0


def main():
    global verbose
    args = parse_args()
    verbose = args.verbose

    if not args.no_flash:
        flash_image(args.aport)
        if args.type != 'solo':
            flash_image(args.bport)

    env = TestEnvironment(args.aport, TestEnvironment.A)
    env_b = None
    cases = env.get_testlist()
    if args.type != 'solo':
        env_b = TestEnvironment(args.bport, TestEnvironment.B)
        cases_b = env_b.get_testlist()
        if cases != cases_b:
            raise TestRunnerError("Test cases on units A & B don't match")

    if args.list:  # if list option is specified, do not run test cases
        print("List of test cases:")
        for test in cases:
            print(test)
        sys.exit(0)

    if args.testcases:  # if testcases is specified run only those cases
        cases = [c for c in cases if str(c.index) in args.testcases]

    sys.exit(0 if run(env, env_b, cases) else 1)


class TestCase(object):
    def __init__(self, index, name, case_type):
        self.name = name
        self.index = index
        self.case_type = case_type

    def __repr__(self):
        return "#%d: %s (%s)" % (self.index, self.name, self.case_type)

    def __eq__(self, other):
        return (self.index == other.index and
                self.name == other.name and
                self.case_type == other.case_type)

    def run(self, env_a, env_b=None):
        """
        Run the test represented by this instance, against the environment(s) passed in.

        Returns a TestResult
        """
        sys.stdout.write("Running test case '%s'...%s" % (self.name, "\n" if verbose else " "*(40-len(self.name))))
        mon_a = env_a.start_testcase(self)
        mon_b = env_b.start_testcase(self) if env_b else None
        while True:
            if mon_a.get_result() and (mon_b is None or mon_b.get_result()):
                break  # all running test environments have finished

            # or, in the case both are running, stop as soon as either environemnt shows a failure
            try:
                if mon_a.get_result().is_failure():
                    mon_b.cancel()
                    break
            except AttributeError:
                pass
            try:
                if mon_b.get_result().is_failure():
                    mon_a.cancel()
                    break
            except AttributeError:
                pass
            time.sleep(0.1)

        if mon_b is not None:
            # return whichever result is more severe
            res = max(mon_a.get_result(), mon_b.get_result())
        else:
            res = mon_a.get_result()
        if not verbose:  # finish the line after the ...
            print(TestResult.STATUS_NAMES[res.status])
            if res.is_failure():
                message = res.message
                if "/" in res.message:  # cut anything before the file name in the failure
                    message = message[message.index("/"):]
                print("FAILURE MESSAGE:\n%s\n" % message)
        return res


class TestResult(object):
    """ Class to wrap a test result code and a message """
    # Test status flags, higher = more severe
    CANCELLED = 0
    SKIPPED = 1
    PASSED = 2
    FAILED = 3
    ERROR = 4

    STATUS_NAMES = {
        CANCELLED: "Cancelled",
        SKIPPED: "Skipped",
        PASSED: "Passed",
        FAILED: "Failed",
        ERROR: "Error"
        }

    def __init__(self, status, message):
        self.status = status
        self.message = message

    def is_failure(self):
        return self.status >= TestResult.FAILED

    def __qe__(self, other):
        if other is None:
            return False
        else:
            return self.status == other.status

    def __lt__(self, other):
        if other is None:
            return False
        else:
            return self.status < other.status


class TestMonitor(object):
    """ Class to monitor a running test case in a separate thread, defer reporting of the result until it's done.

    Can poll for completion by calling is_done(), read a TestResult via .get_result()
    """
    def __init__(self, port, instance):
        super(TestMonitor, self).__init__()
        self._thread = threading.Thread(target=self._monitorThread)
        self._port = port
        self._instance = instance
        self._result = None
        self._cancelled = False
        self.output = ""
        self._thread.start()

    def cancel(self):
        self._cancelled = True

    def is_done(self):
        return self._result is not None

    def get_result(self):
        return self._result

    def _monitorThread(self):
        self.output = ""
        start_time = time.time()
        self._port.timeout = SHORT_OUTPUT_TIMEOUT
        try:
            while not self._cancelled and time.time() < start_time + TESTCASE_TIMEOUT:
                line = self._port.readline().decode("utf-8", "ignore")
                if line == "":
                    continue  # timed out
                self.output += "%s+%4.2fs %s" % (self._instance, time.time()-start_time, line)
                verbose_print(line.strip())
                if line.endswith(":PASS\r\n"):
                    self._result = TestResult(TestResult.PASSED, "Test passed.")
                    return
                elif ":FAIL:" in line:
                    self._result = TestResult(TestResult.FAILED, line)
                    return
                elif line == TESTRUNNER_BANNER:
                    self._result = TestResult(TestResult.ERROR, "Test caused crash and reset.")
                    return
            if not self._cancelled:
                self._result = TestResult(TestResult.CANCELLED, "Cancelled")
            else:
                self._result = TestResult(TestResult.ERROR, "Test timed out")

        finally:
            self._port.timeout = None


class TestEnvironment(object):
    A = "A"
    B = "B"

    def __init__(self, port, instance):
        self._name = port
        self._port = TestSerialPort(port, baudrate=115200)
        self._instance = instance

    def reset(self):
        """ Resets the test board, and waits for the test runner program to start up """
        for i in range(RESET_RETRIES):
            self._port.setDTR(False)
            self._port.setRTS(True)
            time.sleep(0.05)
            self._port.flushInput()
            self._port.setRTS(False)
            verbose_print("Waiting for test runner startup...")
            if self._port.wait_line(lambda line: line == TESTRUNNER_BANNER):
                return
            else:
                verbose_print("Retrying to reset the test board, attempt=%d" %
                              (i + 1))
                continue
            raise TestRunnerError("Port %s failed to start test runner" % self._port)

    def get_testlist(self):
        """ Resets the test board and returns the enumerated list of all supported tests """
        self.reset()
        tests = []
        verbose_print("Enumerating tests...")

        def collect_testcases(line):
                if line.startswith(">"):
                    return True  # prompt means list of test cases is done, success
                m = re.match(r"CASE (\d+) = (.+?) ([A-Z]+)", line)
                if m is not None:
                    t = TestCase(int(m.group(1)), m.group(2), m.group(3).lower())
                    verbose_print(t)
                    tests.append(t)
        if not self._port.wait_line(collect_testcases):
            raise TestRunnerError("Port %s failed to read test list" % self._port)
        verbose_print("Port %s found %d test cases" % (self._name, len(tests)))
        return tests

    def start_testcase(self, case):
        """ Starts the specified test instance and returns a TestMonitor reader thread instance
        to monitor the output
        """
        # synchronously start the test case
        self.reset()
        if not self._port.wait_line(lambda line: line.startswith(">")):
            raise TestRunnerError("Failed to read test runnner prompt")
        command = "%s%d\r\n" % (self._instance, case.index)
        self._port.write(command.encode("utf-8"))
        return TestMonitor(self._port, self._instance)


def get_testdir():
    """
    Return the 'tests' directory in the source tree
    (assuming the test_runner.py script is in that directory.
    """
    res = os.path.dirname(__name__)
    return "." if res == "" else res


def flash_image(serial_port):
    # Bit hacky: rather than calling esptool directly,
    # just use the Makefile flash target with the correct ESPPORT argument
    env = dict(os.environ)
    env["ESPPORT"] = serial_port
    verbose_print("Building and flashing test image to %s..." % serial_port)
    try:
        stdout = sys.stdout if verbose else None
        subprocess.check_call(["make", "flash"], cwd=get_testdir(),
                              stdout=stdout, stderr=subprocess.STDOUT, env=env)
    except subprocess.CalledProcessError as e:
        raise TestRunnerError("'make flash EPPORT=%s' failed with exit code %d" %
                              (serial_port, e.returncode))
    verbose_print("Flashing successful.")


def parse_args():
    parser = argparse.ArgumentParser(description='esp-open-rtos testrunner', prog='test_runner')

    parser.add_argument(
        '--type', '-t',
        help='Type of test hardware attached to serial ports A & (optionally) B',
        choices=['solo', 'dual', 'eyore_test'], default='solo')

    parser.add_argument(
        '--aport', '-a',
        help='Serial port for device A',
        default='/dev/ttyUSB0')

    parser.add_argument(
        '--bport', '-b',
        help='Serial port for device B (ignored if type is \'solo\')',
        default='/dev/ttyUSB1')

    parser.add_argument(
        '--no-flash', '-n',
        help='Don\'t flash the test binary image before running tests',
        action='store_true',
        default=False)

    parser.add_argument(
        '--list', '-l',
        help='Display list of available test cases on a device',
        action='store_true',
        default=False)

    parser.add_argument(
        '--verbose', '-v',
        help='Verbose test runner debugging output',
        action='store_true',
        default=False)

    parser.add_argument('testcases', nargs='*',
                        help='Optional list of test case numbers to run. '
                             'By default, all tests are run.')

    return parser.parse_args()


class TestRunnerError(RuntimeError):
    def __init__(self, message):
        RuntimeError.__init__(self, message)


class TestSerialPort(serial.Serial):
    def __init__(self, *args, **kwargs):
        super(TestSerialPort, self).__init__(*args, **kwargs)

    def wait_line(self, callback, timeout=SHORT_OUTPUT_TIMEOUT):
        """ Wait for the port to output a particular piece of line content, as judged by callback

            Callback called as 'callback(line)' and returns not-True if non-match otherwise can return any value.

            Returns first non-False result from the callback, or None if it timed out waiting for a new line.

            Note that a serial port spewing legitimate lines of output may block this function forever, if callback
            doesn't detect this is happening.
        """
        self.timeout = timeout
        try:
            res = None
            while not res:
                line = self.readline()
                if line == b"":
                    break  # timed out
                line = line.decode("utf-8", "ignore").rstrip()
                res = callback(line)
            return res
        finally:
            self.timeout = None

verbose = False


def verbose_print(msg):
    if verbose:
        print(msg)

if __name__ == '__main__':
    try:
        main()
    except TestRunnerError as e:
        print(e)
        sys.exit(2)

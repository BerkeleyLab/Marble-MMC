from mboxexchange import getPageAndName


def test_getPageAndName(verbose=False):
    tests = (
        ("MB3_FOO",   (3,      "FOO")),
        ("FOO",       (None,   "FOO")),
        ("FOO_3",     (None,   "FOO_3")),
    )
    fails = 0
    for test_input, expected in tests:
        result = getPageAndName(test_input)
        if verbose:
            print(f"getPageAndName({test_input}) = {result}")
        if result != expected:
            print(f"FAIL: {result} != {expected}")
            fails += 1
    if fails == 0:
        print("PASS")
        return 0
    print(f"{fails}/{len(tests)} tests failed")
    return 1


def do_tests(verbose=False):
    tests = (
        test_getPageAndName,
    )
    rval = 0
    for test in tests:
        rval |= test(verbose=verbose)
    return rval


if __name__ == "__main__":
    do_tests(verbose=False)

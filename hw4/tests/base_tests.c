#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include <criterion/criterion.h>

Test(basecode_suite, cook_basic_test, .timeout=20) {
    char *cmd = "ulimit -t 10; python3 tests/test_cook.py -c 2 -f rsrc/eggs_benedict.ckb";

    int return_code = WEXITSTATUS(system(cmd));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
}

Test(basecode_suite, hello_world_test, .timeout=20) {
    char *cmd = "ulimit -t 10; bin/cook -c 1 -f rsrc/hello_world.ckb > hello_world.out";
    char *cmp = "cmp hello_world.out tests/rsrc/hello_world.out";
    int err = mkdir("tmp", 0777);
    if(err == -1 && errno != EEXIST) {
	perror("Could not make tmp directory");
	cr_assert_fail("no tmp directory");
    }

    int return_code = WEXITSTATUS(system(cmd));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
		 return_code);
    return_code = WEXITSTATUS(system(cmp));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program output did not match reference output.");
}

Test(basecode_suite, cook_basic_test_1, .timeout=20) {
    char *cmd = "ulimit -t 10; python3 tests/test_cook.py -c 2 -f rsrc/eggs_benedict.ckb -m hollandaise_sauce";

    int return_code = WEXITSTATUS(system(cmd));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
         return_code);
}

Test(basecode_suite, cook_basic_test_2, .timeout=20) {
    char *cmd = "ulimit -t 10; python3 tests/test_cook.py -c 2 -f rsrc/eggs_benedict.ckb -c 5";

    int return_code = WEXITSTATUS(system(cmd));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
         return_code);
}


Test(basecode_suite, cook_basic_test_3, .timeout=20) {
    char *cmd = "ulimit -t 10; python3 tests/test_cook.py -c 2 -f rsrc/eggs_benedict.ckb -c 10 -m cream";

    int return_code = WEXITSTATUS(system(cmd));
    cr_assert_eq(return_code, EXIT_SUCCESS,
                 "Program exited with %d instead of EXIT_SUCCESS",
         return_code);
}


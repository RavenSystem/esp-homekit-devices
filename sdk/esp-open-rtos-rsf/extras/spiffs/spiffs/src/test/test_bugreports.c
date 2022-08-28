/*
 * test_bugreports.c
 *
 *  Created on: Mar 8, 2015
 *      Author: petera
 */



#include "testrunner.h"
#include "test_spiffs.h"
#include "spiffs_nucleus.h"
#include "spiffs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>

SUITE(bug_tests)
static void setup() {
  _setup_test_only();
}
static void teardown() {
  _teardown();
}

TEST(nodemcu_full_fs_1) {
  fs_reset_specific(0, 0, 4096*20, 4096, 4096, 256);

  int res;
  spiffs_file fd;

  printf("  fill up system by writing one byte a lot\n");
  fd = SPIFFS_open(FS, "test1.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  int i;
  spiffs_stat s;
  res = SPIFFS_OK;
  for (i = 0; i < 100*1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }

  int errno = SPIFFS_errno(FS);
  int res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);

  TEST_CHECK(errno == SPIFFS_ERR_FULL);
  SPIFFS_close(FS, fd);

  printf("  remove big file\n");
  res = SPIFFS_remove(FS, "test1.txt");

  printf("res:%i errno:%i\n",res, SPIFFS_errno(FS));

  TEST_CHECK(res == SPIFFS_OK);
  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FILE_CLOSED);
  res2 = SPIFFS_stat(FS, "test1.txt", &s);
  TEST_CHECK(res2 < 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_NOT_FOUND);

  printf("  create small file\n");
  fd = SPIFFS_open(FS, "test2.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  res = SPIFFS_OK;
  for (i = 0; res >= 0 && i < 1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }
  TEST_CHECK(res >= SPIFFS_OK);

  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);

  TEST_CHECK(s.size == 1000);
  SPIFFS_close(FS, fd);

  return TEST_RES_OK;

} TEST_END

TEST(nodemcu_full_fs_2) {
  fs_reset_specific(0, 0, 4096*22, 4096, 4096, 256);

  int res;
  spiffs_file fd;

  printf("  fill up system by writing one byte a lot\n");
  fd = SPIFFS_open(FS, "test1.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  int i;
  spiffs_stat s;
  res = SPIFFS_OK;
  for (i = 0; i < 100*1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }

  int errno = SPIFFS_errno(FS);
  int res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);

  TEST_CHECK(errno == SPIFFS_ERR_FULL);
  SPIFFS_close(FS, fd);

  res2 = SPIFFS_stat(FS, "test1.txt", &s);
  TEST_CHECK(res2 == SPIFFS_OK);

  SPIFFS_clearerr(FS);
  printf("  create small file\n");
  fd = SPIFFS_open(FS, "test2.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
#if 0
  // before gc in v3.1
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_OK);
  TEST_CHECK(fd > 0);

  for (i = 0; i < 1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }

  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FULL);
  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);
  TEST_CHECK(s.size == 0);
  SPIFFS_clearerr(FS);
#else
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_ERR_FULL);
  SPIFFS_clearerr(FS);
#endif
  printf("  remove files\n");
  res = SPIFFS_remove(FS, "test1.txt");
  TEST_CHECK(res == SPIFFS_OK);
#if 0
  res = SPIFFS_remove(FS, "test2.txt");
  TEST_CHECK(res == SPIFFS_OK);
#endif

  printf("  create medium file\n");
  fd = SPIFFS_open(FS, "test3.txt", SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_OK);
  TEST_CHECK(fd > 0);

  for (i = 0; i < 20*1000; i++) {
    u8_t buf = 'x';
    res = SPIFFS_write(FS, fd, &buf, 1);
  }
  TEST_CHECK(SPIFFS_errno(FS) == SPIFFS_OK);

  res2 = SPIFFS_fstat(FS, fd, &s);
  TEST_CHECK(res2 == SPIFFS_OK);
  printf("  >>> file %s size: %i\n", s.name, s.size);
  TEST_CHECK(s.size == 20*1000);

  return TEST_RES_OK;

} TEST_END

TEST(magic_test) {
  // this test only works on default sizes
  TEST_ASSERT(sizeof(spiffs_obj_id) == sizeof(u16_t));

  // one obj lu page, not full
  fs_reset_specific(0, 0, 4096*16, 4096, 4096*1, 128);
  TEST_CHECK(SPIFFS_CHECK_MAGIC_POSSIBLE(FS));
  // one obj lu page, full
  fs_reset_specific(0, 0, 4096*16, 4096, 4096*2, 128);
  TEST_CHECK(!SPIFFS_CHECK_MAGIC_POSSIBLE(FS));
  // two obj lu pages, not full
  fs_reset_specific(0, 0, 4096*16, 4096, 4096*4, 128);
  TEST_CHECK(SPIFFS_CHECK_MAGIC_POSSIBLE(FS));

  return TEST_RES_OK;

} TEST_END

TEST(nodemcu_309) {
  fs_reset_specific(0, 0, 4096*20, 4096, 4096, 256);

  int res;
  spiffs_file fd;
  int j;

  for (j = 1; j <= 3; j++) {
    char fname[32];
    sprintf(fname, "20K%i.txt", j);
    fd = SPIFFS_open(FS, fname, SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_DIRECT, 0);
    TEST_CHECK(fd > 0);
    int i;
    spiffs_stat s;
    res = SPIFFS_OK;
    u8_t err = 0;
    for (i = 1; i <= 1280; i++) {
      char *buf = "0123456789ABCDE\n";
      res = SPIFFS_write(FS, fd, buf, strlen(buf));
      if (!err && res < 0) {
        printf("err @ %i,%i\n", i, j);
        err = 1;
      }
    }
  }

  int errno = SPIFFS_errno(FS);
  TEST_CHECK(errno == SPIFFS_ERR_FULL);

  u32_t total;
  u32_t used;

  SPIFFS_info(FS, &total, &used);
  printf("total:%i\nused:%i\nremain:%i\nerrno:%i\n", total, used, total-used, errno);
  //TEST_CHECK(total-used < 11000); // disabled, depends on too many variables

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  SPIFFS_opendir(FS, "/", &d);
  int spoon_guard = 0;
  while ((pe = SPIFFS_readdir(&d, pe))) {
    printf("%s [%04x] size:%i\n", pe->name, pe->obj_id, pe->size);
    TEST_CHECK(spoon_guard++ < 3);
  }
  TEST_CHECK(spoon_guard == 3);
  SPIFFS_closedir(&d);

  return TEST_RES_OK;

} TEST_END


TEST(robert) {
  // create a clean file system starting at address 0, 2 megabytes big,
  // sector size 65536, block size 65536, page size 256
  fs_reset_specific(0, 0, 1024*1024*2, 65536, 65536, 256);

  int res;
  spiffs_file fd;
  char fname[32];

  sprintf(fname, "test.txt");
  fd = SPIFFS_open(FS, fname, SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC, 0);
  TEST_CHECK(fd > 0);
  int i;
  res = SPIFFS_OK;
  char buf[500];
  memset(buf, 0xaa, 500);
  res = SPIFFS_write(FS, fd, buf, 500);
  TEST_CHECK(res >= SPIFFS_OK);
  SPIFFS_close(FS, fd);

  int errno = SPIFFS_errno(FS);
  TEST_CHECK(errno == SPIFFS_OK);

  //SPIFFS_vis(FS);
  // unmount
  SPIFFS_unmount(FS);

  // remount
  res = fs_mount_specific(0, 1024*1024*2, 65536, 65536, 256);
  TEST_CHECK(res== SPIFFS_OK);

  //SPIFFS_vis(FS);

  spiffs_stat s;
  TEST_CHECK(SPIFFS_stat(FS, fname, &s) == SPIFFS_OK);
  printf("file %s stat size %i\n", s.name, s.size);
  TEST_CHECK(s.size == 500);

  return TEST_RES_OK;

} TEST_END


TEST(spiffs_12) {
  fs_reset_specific(0x4024c000, 0x4024c000 + 0, 192*1024, 4096, 4096*2, 256);

  int res;
  spiffs_file fd;
  int j = 1;

  while (1) {
    char fname[32];
    sprintf(fname, "file%i.txt", j);
    fd = SPIFFS_open(FS, fname, SPIFFS_RDWR | SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_DIRECT, 0);
    if (fd <=0) break;

    int i;
    res = SPIFFS_OK;
    for (i = 1; i <= 100; i++) {
      char *buf = "0123456789ABCDE\n";
      res = SPIFFS_write(FS, fd, buf, strlen(buf));
      if (res < 0) break;
    }
    SPIFFS_close(FS, fd);
    j++;
  }

  int errno = SPIFFS_errno(FS);
  TEST_CHECK(errno == SPIFFS_ERR_FULL);

  u32_t total;
  u32_t used;

  SPIFFS_info(FS, &total, &used);
  printf("total:%i (%iK)\nused:%i (%iK)\nremain:%i (%iK)\nerrno:%i\n", total, total/1024, used, used/1024, total-used, (total-used)/1024, errno);

  spiffs_DIR d;
  struct spiffs_dirent e;
  struct spiffs_dirent *pe = &e;

  SPIFFS_opendir(FS, "/", &d);
  while ((pe = SPIFFS_readdir(&d, pe))) {
    printf("%s [%04x] size:%i\n", pe->name, pe->obj_id, pe->size);
  }
  SPIFFS_closedir(&d);

  //SPIFFS_vis(FS);

  //dump_page(FS, 0);
  //dump_page(FS, 1);

  return TEST_RES_OK;

} TEST_END


TEST(zero_sized_file_44) {
  fs_reset();

  spiffs_file fd = SPIFFS_open(FS, "zero", SPIFFS_RDWR | SPIFFS_CREAT, 0);
  TEST_CHECK_GE(fd, 0);

  int res = SPIFFS_close(FS, fd);
  TEST_CHECK_GE(res, 0);

  fd = SPIFFS_open(FS, "zero", SPIFFS_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  u8_t buf[8];
  res = SPIFFS_read(FS, fd, buf, 8);
  TEST_CHECK_LT(res, 0);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_END_OF_OBJECT);

  res = SPIFFS_read(FS, fd, buf, 0);
  TEST_CHECK_GE(res, 0);

  res = SPIFFS_read(FS, fd, buf, 0);
  TEST_CHECK_GE(res, 0);

  buf[0] = 1;
  buf[1] = 2;

  res = SPIFFS_write(FS, fd, buf, 2);
  TEST_CHECK_EQ(res, 2);

  res = SPIFFS_lseek(FS, fd, 0, SPIFFS_SEEK_SET);
  TEST_CHECK_GE(res, 0);

  u8_t b;
  res = SPIFFS_read(FS, fd, &b, 1);
  TEST_CHECK_EQ(res, 1);
  TEST_CHECK_EQ(b, 1);

  res = SPIFFS_read(FS, fd, &b, 1);
  TEST_CHECK_EQ(res, 1);
  TEST_CHECK_EQ(b, 2);

  res = SPIFFS_read(FS, fd, buf, 8);
  TEST_CHECK_LT(res, 0);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_END_OF_OBJECT);

  return TEST_RES_OK;
} TEST_END

#if !SPIFFS_READ_ONLY
TEST(truncate_48) {
  fs_reset();

  u32_t len = SPIFFS_DATA_PAGE_SIZE(FS)/2;

  s32_t res = test_create_and_write_file("small", len, len);
  TEST_CHECK_GE(res, 0);

  spiffs_file fd = SPIFFS_open(FS, "small", SPIFFS_RDWR, 0);
  TEST_CHECK_GE(fd, 0);

  spiffs_fd *desc;
#if SPIFFS_FILEHDL_OFFSET
  res = spiffs_fd_get(FS, fd - TEST_SPIFFS_FILEHDL_OFFSET, &desc);
#else
  res = spiffs_fd_get(FS, fd, &desc);
#endif

  TEST_CHECK_GE(res, 0);

  TEST_CHECK_EQ(desc->size, len);

  u32_t new_len = len/2;
  res = spiffs_object_truncate(desc, new_len, 0);
  TEST_CHECK_GE(res, 0);

  TEST_CHECK_EQ(desc->size, new_len);

  res = SPIFFS_close(FS, fd);
  TEST_CHECK_GE(res, 0);

  spiffs_stat s;
  res = SPIFFS_stat(FS, "small", &s);
  TEST_CHECK_GE(res, 0);
  TEST_CHECK_EQ(s.size, new_len);

  res = SPIFFS_remove(FS, "small");
  TEST_CHECK_GE(res, 0);

  fd = SPIFFS_open(FS, "small", SPIFFS_RDWR, 0);
  TEST_CHECK_LT(fd, 0);
  TEST_CHECK_EQ(SPIFFS_errno(FS), SPIFFS_ERR_NOT_FOUND);

  return TEST_RES_OK;
} TEST_END
#endif

TEST(eof_tell_72) {
  fs_reset();

  s32_t res;

  spiffs_file fd = SPIFFS_open(FS, "file", SPIFFS_CREAT | SPIFFS_RDWR | SPIFFS_APPEND, 0);
  TEST_CHECK_GT(fd, 0);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 1);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 0);

  res = SPIFFS_write(FS, fd, "test", 4);
  TEST_CHECK_EQ(res, 4);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 1);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 4);

  res = SPIFFS_fflush(FS, fd);
  TEST_CHECK_EQ(res, SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 1);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 4);

  res = SPIFFS_lseek(FS, fd, 2, SPIFFS_SEEK_SET);
  TEST_CHECK_EQ(res, 2);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 0);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 2);

  res = SPIFFS_write(FS, fd, "test", 4);
  TEST_CHECK_EQ(res, 4);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 1);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 8);

  res = SPIFFS_fflush(FS, fd);
  TEST_CHECK_EQ(res, SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 1);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 8);

  res = SPIFFS_close(FS, fd);
  TEST_CHECK_EQ(res, SPIFFS_OK);
  TEST_CHECK_LT(SPIFFS_eof(FS, fd), SPIFFS_OK);
  TEST_CHECK_LT(SPIFFS_tell(FS, fd), SPIFFS_OK);

  fd = SPIFFS_open(FS, "file", SPIFFS_RDWR, 0);
  TEST_CHECK_GT(fd, 0);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 0);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 0);

  res = SPIFFS_lseek(FS, fd, 2, SPIFFS_SEEK_SET);
  TEST_CHECK_EQ(res, 2);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 0);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 2);

  res = SPIFFS_write(FS, fd, "test", 4);
  TEST_CHECK_EQ(res, 4);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 0);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 6);

  res = SPIFFS_fflush(FS, fd);
  TEST_CHECK_EQ(res, SPIFFS_OK);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 0);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 6);

  res = SPIFFS_lseek(FS, fd, 0, SPIFFS_SEEK_END);
  TEST_CHECK_EQ(res, 8);
  TEST_CHECK_EQ(SPIFFS_eof(FS, fd), 1);
  TEST_CHECK_EQ(SPIFFS_tell(FS, fd), 8);

  return TEST_RES_OK;
} TEST_END

TEST(spiffs_dup_file_74) {
  fs_reset_specific(0, 0, 64*1024, 4096, 4096*2, 256);
  {
    spiffs_file fd = SPIFFS_open(FS, "/config", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_WRONLY, 0);
    TEST_CHECK(fd >= 0);
    char buf[5];
    strncpy(buf, "test", sizeof(buf));
    SPIFFS_write(FS, fd, buf, 4);
    SPIFFS_close(FS, fd);
  }
  {
    spiffs_file fd = SPIFFS_open(FS, "/data", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_WRONLY, 0);
    TEST_CHECK(fd >= 0);
    SPIFFS_close(FS, fd);
  }
  {
    spiffs_file fd = SPIFFS_open(FS, "/config", SPIFFS_RDONLY, 0);
    TEST_CHECK(fd >= 0);
    char buf[5];
    int cb = SPIFFS_read(FS, fd, buf, sizeof(buf));
    TEST_CHECK(cb > 0 && cb < sizeof(buf));
    TEST_CHECK(strncmp("test", buf, cb) == 0);
    SPIFFS_close(FS, fd);
  }
  {
    spiffs_file fd = SPIFFS_open(FS, "/data", SPIFFS_CREAT | SPIFFS_TRUNC | SPIFFS_WRONLY, 0);
    TEST_CHECK(fd >= 0);
    spiffs_stat stat;
    SPIFFS_fstat(FS, fd, &stat);
    if (strcmp((const char*) stat.name, "/data") != 0) {
      // oops! lets check the list of files...
      spiffs_DIR dir;
      SPIFFS_opendir(FS, "/", &dir);
      struct spiffs_dirent dirent;
      while (SPIFFS_readdir(&dir, &dirent)) {
        printf("%s\n", dirent.name);
      }
      // this will print "/config" two times
      TEST_CHECK(0);
    }
    SPIFFS_close(FS, fd);
  }
  return TEST_RES_OK;
} TEST_END

TEST(temporal_fd_cache) {
  fs_reset_specific(0, 0, 1024*1024, 4096, 2*4096, 256);
  spiffs_file fd;
  int res;
  (FS)->fd_count = 4;

  char *fcss = "blaha.css";

  char *fhtml[] = {
      "index.html", "cykel.html", "bloja.html", "drivmedel.html", "smorgasbord.html",
      "ombudsman.html", "fubbick.html", "paragrod.html"
  };

  const int hit_probabilities[] = {
      25, 20, 16, 12, 10, 8, 5, 4
  };

  const int runs = 10000;

  // create our webserver files
  TEST_CHECK_EQ(test_create_and_write_file(fcss, 2000, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[0], 4000, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[1], 3000, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[2], 2000, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[3], 1000, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[4], 1500, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[5], 3000, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[6], 2000, 256), SPIFFS_OK);
  TEST_CHECK_EQ(test_create_and_write_file(fhtml[7], 3500, 256), SPIFFS_OK);

  clear_flash_ops_log();

  int run = 0;
  do {
    u8_t buf[256];

    // open & read an html
    int dice = rand() % 100;
    int probability = 0;
    int html_ix = 0;
    do {
      probability += hit_probabilities[html_ix];
      if (dice <= probability) {
        break;
      }
      html_ix++;
    } while(probability < 100);

    TEST_CHECK_EQ(read_and_verify(fhtml[html_ix]), SPIFFS_OK);

    // open & read css
    TEST_CHECK_EQ(read_and_verify(fcss), SPIFFS_OK);
  } while (run ++ < runs);

  return TEST_RES_OK;
} TEST_END

#if 0
TEST(spiffs_hidden_file_90) {
  fs_mount_dump("imgs/90.hidden_file.spiffs", 0, 0, 1*1024*1024, 4096, 4096, 128);

  SPIFFS_vis(FS);

  dump_page(FS, 1);
  dump_page(FS, 0x8fe);
  dump_page(FS, 0x8ff);

  {
    spiffs_DIR dir;
    SPIFFS_opendir(FS, "/", &dir);
    struct spiffs_dirent dirent;
    while (SPIFFS_readdir(&dir, &dirent)) {
      printf("%-32s sz:%-7i obj_id:%08x pix:%08x\n", dirent.name, dirent.size, dirent.obj_id, dirent.pix);
    }
  }

  printf("remove cli.bin res %i\n", SPIFFS_remove(FS, "cli.bin"));

  {
    spiffs_DIR dir;
    SPIFFS_opendir(FS, "/", &dir);
    struct spiffs_dirent dirent;
    while (SPIFFS_readdir(&dir, &dirent)) {
      printf("%-32s sz:%-7i obj_id:%08x pix:%08x\n", dirent.name, dirent.size, dirent.obj_id, dirent.pix);
    }
  }
  return TEST_RES_OK;

} TEST_END
#endif
#if 0
TEST(null_deref_check_93) {
  fs_mount_dump("imgs/93.dump.bin", 0, 0, 2*1024*1024, 4096, 4096, 256);

  //int res = SPIFFS_open(FS, "d43.fw", SPIFFS_TRUNC | SPIFFS_CREAT | SPIFFS_WRONLY, 0);
  //TEST_CHECK_GE(res, SPIFFS_OK);

  SPIFFS_vis(FS);

  printf("\n\n-------------------------------------------------\n\n");

  SPIFFS_check(FS);
  //fs_store_dump("imgs/93.dump.checked.bin");

  SPIFFS_vis(FS);

  printf("\n\n-------------------------------------------------\n\n");

  SPIFFS_check(FS);

  SPIFFS_vis(FS);
  printf("\n\n-------------------------------------------------\n\n");



  return TEST_RES_OK;
} TEST_END
#endif

SUITE_TESTS(bug_tests)
  ADD_TEST(nodemcu_full_fs_1)
  ADD_TEST(nodemcu_full_fs_2)
  ADD_TEST(magic_test)
  ADD_TEST(nodemcu_309)
  ADD_TEST(robert)
  ADD_TEST(spiffs_12)
  ADD_TEST(zero_sized_file_44)
  ADD_TEST(truncate_48)
  ADD_TEST(eof_tell_72)
  ADD_TEST(spiffs_dup_file_74)
  ADD_TEST(temporal_fd_cache)
#if 0
  ADD_TEST(spiffs_hidden_file_90)
#endif
#if 0
  ADD_TEST(null_deref_check_93)
#endif
SUITE_END(bug_tests)

/* FOnline portable launcher. MIT License; see the engine LICENSE file */
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

enum {
  ENTRY,
  LOADER,
  LIBRARIES,
  DATA,
  POLICY,
  PRESERVE_CWD,
  GCONV,
  CA,
  CA_SETTING,
  FIELD_COUNT
};
static const char *field_names[] = {"entry", "loader", "libraries",
                                    "data",  "policy", "preserve_cwd",
                                    "gconv", "ca",     "ca_setting"};

static void fail(const char *message) {
  fprintf(stderr, "FOnline portable launcher: %s: %s\n", message,
          strerror(errno));
  exit(125);
}

static char *join(const char *root, const char *relative) {
  char *value = NULL;
  if (asprintf(&value, "%s/%s", root, relative) < 0)
    fail("allocate path");
  return value;
}

static void read_config(const char *root, char **fields) {
  char *name = join(root, "AppRun.conf");
  FILE *file = fopen(name, "r");
  if (!file)
    fail("read AppRun.conf");
  free(name);
  char *line = NULL;
  size_t capacity = 0;
  ssize_t length;
  while ((length = getline(&line, &capacity, file)) >= 0) {
    while (length && (line[length - 1] == '\n' || line[length - 1] == '\r'))
      line[--length] = 0;
    char *equal = strchr(line, '=');
    if (!equal || length > 65536) {
      errno = EINVAL;
      fail("invalid AppRun.conf");
    }
    *equal++ = 0;
    int index;
    for (index = 0; index < FIELD_COUNT; index++)
      if (!strcmp(line, field_names[index]))
        break;
    if (index == FIELD_COUNT || fields[index]) {
      errno = EINVAL;
      fail("unknown or duplicate launcher field");
    }
    fields[index] = strdup(equal);
    if (!fields[index])
      fail("allocate launcher field");
  }
  if (ferror(file))
    fail("read launcher fields");
  fclose(file);
  free(line);
  for (int index = 0; index < FIELD_COUNT; index++) {
    if (!fields[index]) {
      errno = EINVAL;
      fail("missing launcher field");
    }
  }
  if (strcmp(fields[POLICY], "local") && strcmp(fields[POLICY], "external")) {
    errno = EINVAL;
    fail("invalid writable policy");
  }
  if (strcmp(fields[PRESERVE_CWD], "0") && strcmp(fields[PRESERVE_CWD], "1")) {
    errno = EINVAL;
    fail("invalid working-directory policy");
  }
}

static int has_setting(int argc, char **argv, const char *name) {
  for (int index = 1; index < argc; index++) {
    const char *argument = argv[index];
    if (*argument++ != '-')
      continue;
    if (*argument == '-')
      argument++;
    if (!strcmp(argument, name))
      return 1;
  }
  return 0;
}

static int writable_directory(const char *directory) {
  struct stat info;
  if (mkdir(directory, 0755) && errno != EEXIST)
    return 0;
  if (stat(directory, &info) || !S_ISDIR(info.st_mode))
    return 0;
  char *probe = join(directory, ".fonline-write-XXXXXX");
  int file = mkstemp(probe);
  if (file < 0) {
    free(probe);
    return 0;
  }
  close(file);
  unlink(probe);
  free(probe);
  return 1;
}

static void forward_signal(int number, pid_t primary) {
  int primary_group = primary > 0 && kill(-primary, number) == 0;
  char children_path[96];
  snprintf(children_path, sizeof(children_path), "/proc/self/task/%ld/children",
           (long)getpid());
  FILE *children = fopen(children_path, "r");
  if (!children)
    fail("inspect adopted children");
  long child;
  while (fscanf(children, "%ld", &child) == 1) {
    pid_t group = getpgid((pid_t)child);
    if (primary_group && group == primary)
      continue;
    if (group != child || kill(-(pid_t)child, number))
      kill((pid_t)child, number);
  }
  fclose(children);
}

static int status_code(int status) {
  return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
}

static void terminal_foreground(pid_t group) {
  struct sigaction ignored, previous;
  memset(&ignored, 0, sizeof(ignored));
  ignored.sa_handler = SIG_IGN;
  sigemptyset(&ignored.sa_mask);
  if (sigaction(SIGTTOU, &ignored, &previous))
    fail("protect terminal ownership change");
  int result = tcsetpgrp(STDIN_FILENO, group);
  int saved = errno;
  if (sigaction(SIGTTOU, &previous, NULL))
    fail("restore terminal signal disposition");
  if (result && saved != ENOTTY) {
    errno = saved;
    fail("set terminal foreground group");
  }
}

static int watch_image_parent(void) {
  const char *image = getenv("APPIMAGE");
  if (!image || !*image)
    return 0;
  pid_t parent = getppid();
  char parent_path[64];
  snprintf(parent_path, sizeof(parent_path), "/proc/%ld/exe", (long)parent);
  struct stat image_info, parent_info;
  if (stat(image, &image_info) || stat(parent_path, &parent_info) ||
      image_info.st_dev != parent_info.st_dev ||
      image_info.st_ino != parent_info.st_ino)
    return 0;
  /* Extract-and-run has an outer runtime process that does not forward its
   * termination signal */
  if (prctl(PR_SET_PDEATHSIG, SIGTERM))
    fail("watch AppImage runtime");
  return getppid() != parent;
}

static int supervise(char **arguments) {
  if (prctl(PR_SET_CHILD_SUBREAPER, 1))
    fail("enable child supervision");
  sigset_t blocked, previous;
  sigemptyset(&blocked);
  const int signals[] = {SIGTERM, SIGINT, SIGHUP, SIGQUIT, SIGCHLD};
  struct sigaction action;
  memset(&action, 0, sizeof(action));
  action.sa_handler = SIG_DFL;
  sigemptyset(&action.sa_mask);
  for (size_t index = 0; index < sizeof(signals) / sizeof(signals[0]);
       index++) {
    sigaddset(&blocked, signals[index]);
    if (sigaction(signals[index], &action, NULL))
      fail("install signal handler");
  }
  if (sigprocmask(SIG_BLOCK, &blocked, &previous))
    fail("block startup signals");
  if (watch_image_parent())
    return 128 + SIGTERM;
  pid_t terminal_group = tcgetpgrp(STDIN_FILENO) == getpgrp() ? getpgrp() : -1;
  int startup[2];
  if (pipe2(startup, O_CLOEXEC))
    fail("create application startup barrier");
  pid_t primary = fork();
  if (primary < 0)
    fail("fork application");
  if (!primary) {
    close(startup[1]);
    if (setpgid(0, 0))
      fail("create application process group");
    char ready;
    ssize_t size;
    do {
      size = read(startup[0], &ready, 1);
    } while (size < 0 && errno == EINTR);
    if (size != 1)
      _exit(125);
    close(startup[0]);
    for (size_t index = 0; index < sizeof(signals) / sizeof(signals[0]);
         index++)
      signal(signals[index], SIG_DFL);
    if (sigprocmask(SIG_SETMASK, &previous, NULL))
      fail("restore application signals");
    execv(arguments[0], arguments);
    fprintf(stderr, "FOnline portable launcher: exec %s: %s\n", arguments[0],
            strerror(errno));
    _exit(127);
  }
  close(startup[0]);
  if (setpgid(primary, primary) && errno != EACCES && errno != ESRCH)
    fail("set application process group");
  if (terminal_group >= 0)
    terminal_foreground(primary);
  if (write(startup[1], "1", 1) != 1)
    fail("release application startup barrier");
  close(startup[1]);
  int result = 0;
  int shutdown_signal = 0;
  pid_t live_primary = primary;
  for (;;) {
    int status;
    pid_t reaped;
    while ((reaped = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
      if (WIFSTOPPED(status)) {
        if (reaped == primary) {
          if (terminal_group >= 0)
            terminal_foreground(terminal_group);
          if (raise(SIGSTOP))
            fail("suspend terminal supervisor");
          if (tcgetpgrp(STDIN_FILENO) == getpgrp()) {
            terminal_group = getpgrp();
            terminal_foreground(primary);
          }
          kill(-primary, SIGCONT);
        }
        continue;
      }
      if (reaped == primary && terminal_group >= 0) {
        terminal_foreground(terminal_group);
        terminal_group = -1;
      }
      int code = status_code(status);
      if (code && (!result || reaped == primary))
        result = code;
      if (reaped == primary)
        live_primary = 0;
      /* A daemon can be adopted after leaving its original process group */
      if (shutdown_signal)
        forward_signal(shutdown_signal, live_primary);
    }
    if (reaped < 0 && errno == ECHILD)
      return result;
    if (reaped < 0 && errno != EINTR)
      fail("wait for application descendants");
    int number = sigwaitinfo(&blocked, NULL);
    if (number < 0) {
      if (errno == EINTR)
        continue;
      fail("wait for application signal");
    }
    if (number != SIGCHLD) {
      shutdown_signal = number;
      forward_signal(number, live_primary);
    }
  }
}

int main(int argc, char **argv) {
  char root[PATH_MAX];
  ssize_t length = readlink("/proc/self/exe", root, sizeof(root) - 1);
  if (length <= 0 || length >= (ssize_t)sizeof(root) - 1)
    fail("locate AppRun");
  root[length] = 0;
  char *slash = strrchr(root, '/');
  if (!slash) {
    errno = EINVAL;
    fail("invalid AppRun location");
  }
  *slash = 0;
  char *fields[FIELD_COUNT] = {0};
  read_config(root, fields);
  char *loader = join(root, fields[LOADER]);
  char *entry = join(root, fields[ENTRY]);
  char *data = join(root, fields[DATA]);
  char *ca = join(root, fields[CA]);

  /* --library-path applies inside this process without contaminating external
   * helper executables */
  unsetenv("LD_LIBRARY_PATH");
  unsetenv("LD_PRELOAD");
  unsetenv("LD_AUDIT");
  if (!getenv("SSL_CERT_FILE") && setenv("SSL_CERT_FILE", ca, 1))
    fail("set CA bundle");
  char *alsa_plugins = join(root, "usr/bin/alsa-lib");
  char *alsa_data = join(root, "usr/share/alsa");
  if (!access(alsa_plugins, F_OK) && !getenv("ALSA_PLUGIN_DIR") &&
      setenv("ALSA_PLUGIN_DIR", alsa_plugins, 1))
    fail("set ALSA plugin directory");
  if (!access(alsa_data, F_OK) && !getenv("ALSA_CONFIG_DIR") &&
      setenv("ALSA_CONFIG_DIR", alsa_data, 1))
    fail("set ALSA data directory");
  free(alsa_plugins);
  free(alsa_data);
  if (*fields[GCONV]) {
    char *gconv = join(root, fields[GCONV]);
    if (setenv("GCONV_PATH", gconv, 1))
      fail("set conversion modules");
    free(gconv);
  }
  char **arguments = calloc((size_t)argc + 16, sizeof(char *));
  if (!arguments)
    fail("allocate application arguments");
  int count = 0;
  arguments[count++] = loader;
  arguments[count++] = "--inhibit-cache";
  arguments[count++] = "--library-path";
  arguments[count++] = fields[LIBRARIES];
  arguments[count++] = entry;
  for (int index = 1; index < argc; index++)
    arguments[count++] = argv[index];
  if (!has_setting(argc, argv, "Common.UserWritablePath")) {
    char *writable = join(root, "data");
    arguments[count++] = "--Common.UserWritablePath";
    arguments[count++] =
        !strcmp(fields[POLICY], "local") && writable_directory(writable)
            ? writable
            : "*";
  }
  if (*fields[CA_SETTING] && !has_setting(argc, argv, fields[CA_SETTING])) {
    char *option = NULL;
    if (asprintf(&option, "--%s", fields[CA_SETTING]) < 0)
      fail("allocate CA setting");
    arguments[count++] = option;
    arguments[count++] = getenv("SSL_CERT_FILE");
  }
  if (!strcmp(fields[PRESERVE_CWD], "0") && chdir(data))
    fail("enter packaged resource directory");
  return supervise(arguments);
}

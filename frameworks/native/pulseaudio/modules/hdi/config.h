/*
 * Copyright (c) 2024 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef PULSEAUDIO_HDI_CONFIG_H
#define PULSEAUDIO_HDI_CONFIG_H

#define DISABLE_ORC 1

#define ENABLE_LEGACY_DATABASE_ENTRY_FORMAT 1

#define HAVE_ACCEPT4 1

#define HAVE_ALSA 1

#define HAVE_ALSA_UCM 1

#define HAVE_ARPA_INET_H 1

#define HAVE_ATOMIC_BUILTINS 1

#define HAVE_ATOMIC_BUILTINS_MEMORY_MODEL 1

#define HAVE_BYTESWAP_H 1

#define HAVE_CLOCK_GETTIME 1

#define HAVE_COREAUDIO 0

#define HAVE_CTIME_R 1

#define HAVE_DECL_ENVIRON 1

#define HAVE_DECL_SOUND_PCM_READ_BITS 1

#define HAVE_DECL_SOUND_PCM_READ_CHANNELS 1

#define HAVE_DECL_SOUND_PCM_READ_RATE 1

#define HAVE_DLADDR 1

#define HAVE_DLFCN_H 1

#define HAVE_FAST_64BIT_OPERATIONS 1

#define HAVE_FCHMOD 1

#define HAVE_FCHOWN 1

#define HAVE_FORK 1

#define HAVE_FSTAT 1

#define HAVE_GETADDRINFO 1

#define HAVE_GETGRGID_R 1

#define HAVE_GETGRNAM_R 1

#define HAVE_GETPWNAM_R 1

#define HAVE_GETPWUID_R 1

#define HAVE_GETTIMEOFDAY 1

#define HAVE_GETUID 1

#define HAVE_GRP_H 1

#define HAVE_HAL_COMPAT 1

#define HAVE_ICONV 1

#define HAVE_IPV6 1

#define HAVE_LANGINFO_H 1

#define HAVE_LINUX_SOCKIOS_H 1

#define HAVE_LOCALE_H 1

#define HAVE_LRINTF 1

#define HAVE_LSTAT 1

#define HAVE_MEMFD_CREATE 1

#define HAVE_MKFIFO 1

#define HAVE_MLOCK 1

/* Compiler supports mmx. */
#define HAVE_MMX 1

#define HAVE_NANOSLEEP 1

#define HAVE_NETDB_H 1

#define HAVE_NETINET_IN_H 1

#define HAVE_NETINET_IN_SYSTM_H 1

#define HAVE_NETINET_IP_H 1

#define HAVE_NETINET_TCP_H 1

#define HAVE_OPEN64 1

#define HAVE_OPENSSL 1

#define HAVE_OSS_OUTPUT 1

#define HAVE_OSS_WRAPPER 1

#define HAVE_PCREPOSIX_H 1

#define HAVE_PIPE 1

#define HAVE_PIPE2 1

#define HAVE_POLL_H 1

#define HAVE_POSIX_FADVISE 1

#define HAVE_POSIX_MADVISE 1

#define HAVE_POSIX_MEMALIGN 1

#define HAVE_PPOLL 1

#define HAVE_PTHREAD 1

#define HAVE_PTHREAD_GETNAME_NP 1

#define HAVE_PTHREAD_PRIO_INHERIT 1

#define HAVE_PTHREAD_SETAFFINITY_NP 1

#define HAVE_PTHREAD_SETNAME_NP 1

#define HAVE_PWD_H 1

#define HAVE_READLINK 1

#define HAVE_REGEX_H 1

#define HAVE_RUNNING_FROM_BUILD_TREE 1

#define HAVE_SCHED_H 1

#define HAVE_SETEGID 1

#define HAVE_SETEUID 1

#define HAVE_SETPGID 1

#define HAVE_SETREGID 1

#define HAVE_SETRESGID 1

#define HAVE_SETRESUID 1

#define HAVE_SETREUID 1

#define HAVE_SETSID 1

#define HAVE_SIGACTION 1

#define HAVE_SIGXCPU 1

/* Compiler supports sse. */
#define HAVE_SSE 1

#define HAVE_STDINT_H 1

#define HAVE_STD_BOOL 1

#define HAVE_STRERROR_R 1

#define HAVE_STRTOD_L 1

#define HAVE_STRTOF 1

#define HAVE_SYMLINK 1

#define HAVE_SYSCONF 1

#define HAVE_SYSLOG_H 1

#define HAVE_SYS_EVENTFD_H 1

#define HAVE_SYS_IOCTL_H 1

#define HAVE_SYS_MMAN_H 1

#define HAVE_SYS_PRCTL_H 1

#define HAVE_SYS_RESOURCE_H 1

#define HAVE_SYS_SELECT_H 1

#define HAVE_SYS_SOCKET_H 1

#define HAVE_SYS_SYSCALL_H 1

#define HAVE_SYS_UIO_H 1

#define HAVE_SYS_UN_H 1

#define HAVE_SYS_WAIT_H 1

#define HAVE_UNAME 1

#define HAVE_WAVEOUT 0

#define ICONV_CONST

#define LIBICONV_PLUG 1

#define MESON_BUILD 1

#define PACKAGE "pulseaudio"

#define PACKAGE_NAME "pulseaudio"

#define PACKAGE_VERSION "14.0-271-g1a19"

#define PA_ACCESS_GROUP "pulse-access"

#define PA_ACCESS_GROUP "pulse-access"

#define PA_ALSA_PATHS_DIR "/usr/local/share/pulseaudio/alsa-mixer/paths"

#define PA_ALSA_PROFILE_SETS_DIR "/usr/local/share/pulseaudio/alsa-mixer/profile-sets"

#define PA_API_VERSION 12

#define PA_BINARY "/system/bin"

#define PA_BUILDDIR "/home/workspace/pa/pulseaudio/confgure"

#define PA_CFLAGS "Not yet supported on meson"

#define PA_DEFAULT_CONFIG_DIR "/system/etc/pulse"

#define PA_DEFAULT_CONFIG_DIR_UNQUOTED /usr/local/etc/pulse

#define PA_DLSEARCHPATH "/system/lib"

#define PA_INCDIR /usr/local/include

#define PA_LIBDIR /usr/local/lib/x86_64-linux-gnu

#define PA_MACHINE_ID "/usr/local/etc/machine-id"

#define PA_MACHINE_ID_FALLBACK "/var/local/lib/dbus/machine-id"

#define PA_MAJOR 14

#define PA_MINOR 0

#define PA_PROTOCOL_VERSION 35

#define PA_SOEXT ".so"

#define PA_SYSTEM_CONFIG_PATH "/var/local/lib/pulse"

#define PA_SYSTEM_GROUP "pulse"

#define PA_SYSTEM_RUNTIME_PATH "/data/data/.pulse_dir/runtime"

#define PA_SYSTEM_STATE_PATH "/data/data/.pulse_dir/state"

#define PA_SYSTEM_USER "pulse"

#define PULSEDSP_LOCATION /usr/local/lib/x86_64-linux-gnu/pulseaudio

#define PULSE_LOCALEDIR "/usr/local/share/locale"

#endif

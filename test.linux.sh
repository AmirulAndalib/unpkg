#!/bin/sh

set -euf; unset -v IFS; export LC_ALL=C

echo() {
  printf '%s\n' "$*"
}

exe='./pbzx'
test_dir='./tmp'
data_zip='./data.zip'
data_dir="${test_dir}/data"

exitcode=0

check() {
  title="$1" raw="$2" archive="$3" a="$4" b="$5"
  echo "${title}"
  cat <"${raw}" >"${a}"
  if "${exe}" <"${archive}" >"${b}" && diff -q "${a}" "${b}"; then
    echo "${title} -- OK"
    rm -- "${a}" "${b}"
  else
    echo "${title} -- FAILED"
    exitcode=1
  fi
}

mkdir -p -- "${test_dir}"

unzip "${data_zip}" -d "${test_dir}" >/dev/null

check '[TEST] pbzx single-block 1 -- block size: 1024' \
      "${data_dir}/zero1" \
      "${data_dir}/zero1.lzma" \
      "${test_dir}/single-block.1.a" \
      "${test_dir}/single-block.1.b"

check '[TEST] pbzx single-block 2 -- block size: 1024' \
      "${data_dir}/rand1" \
      "${data_dir}/rand1.lzma" \
      "${test_dir}/single-block.2.a" \
      "${test_dir}/single-block.2.b"

check '[TEST] pbzx multi-block 1 -- block size: 1024' \
      "${data_dir}/zero2" \
      "${data_dir}/zero2.lzma" \
      "${test_dir}/multi-block.1.a" \
      "${test_dir}/multi-block.1.b"

check '[TEST] pbzx multi-block 2 -- block size: 1024' \
      "${data_dir}/rand2" \
      "${data_dir}/rand2.lzma" \
      "${test_dir}/multi-block.2.a" \
      "${test_dir}/multi-block.2.b"

if [ 0 -eq "${exitcode}" ]; then
  rm -R -- "${test_dir}"
fi

exit "${exitcode}"

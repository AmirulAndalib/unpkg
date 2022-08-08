#!/bin/sh

set -euf; unset -v IFS; export LC_ALL=C

exec >&2

echo() {
  printf '%s\n' "$*"
}

test_dir='./tests'

src_dir='/System/Library/Frameworks/CoreServices.framework'
dst_dir="${test_dir}/$(basename -- ${src_dir})"

mkdir -p -- "${test_dir}"

for exe in pbzx pbzy pbzz; do
  for algo in lzma lz4 lzfse zlib; do
    if [ pbzx = "${exe}" ] && [ lzma != "${algo}" ]; then
      continue
    fi
    echo "[TEST] ${exe} compression algorithm: ${algo}"
    raw="${test_dir}/payload"
    archive="${raw}.${algo}"
    yaa archive -a "${algo}" -i "${src_dir}" -o "${archive}"
    if ! "./${exe}" "${archive}" >"${raw}"; then
      echo "[TEST] ${exe} decompression failed: ${algo}"
      if [ pbzz:zlib = "${exe}:${algo}" ]; then continue; fi
      exit 1
    fi
    yaa extract -a raw -d "${test_dir}" <"${raw}"
    diff -q -r "${src_dir}" "${dst_dir}"
    rm -R -- "${raw}" "${archive}" "${dst_dir}"
    echo "[TEST] ${exe} decompression succeeded: ${algo}"
  done
done

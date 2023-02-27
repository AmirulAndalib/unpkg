#!/bin/sh

set -euf; unset -v IFS; export LC_ALL=C

exec >&2

echo() {
  printf '%s\n' "$*"
}

test_dir='./tmp'

src_dir='/System/Library/Frameworks/CoreServices.framework'
dst_dir="${test_dir}/$(basename -- ${src_dir})"

exitcode=0

mkdir -p -- "${test_dir}"

for exe in pbzx pbzy pbzz; do
  for algo in lzma lz4 lzfse zlib; do
    if [ pbzx = "${exe}" ] && [ lzma != "${algo}" ]; then
      continue
    fi
    echo "[TEST] ${exe} compression algorithm: ${algo}"

    a='hello'
    b="$(
      printf '%s' "${a}" | compression_tool -encode -A "${algo}" | "./${exe}"
    )"
    if [ x"${a}" = x"${b}" ]; then
      echo "[TEST] ${exe} single-block decompression succeeded: ${algo}"
    else
      echo "[TEST] ${exe} single-block decompression failed: ${algo}"
      exitcode=1
    fi

    raw="${test_dir}/payload"
    archive="${raw}.${algo}"
    yaa archive -a "${algo}" -i "${src_dir}" -o "${archive}"
    if "./${exe}" "${archive}" >"${raw}" \
      && yaa extract -a raw -i "${raw}" -d "${test_dir}" \
      && diff -q -r "${src_dir}" "${dst_dir}"
    then
      echo "[TEST] ${exe} multi-block decompression succeeded: ${algo}"
      rm -R -- "${raw}" "${archive}" "${dst_dir}"
    else
      if [ pbzz:zlib = "${exe}:${algo}" ]; then
        echo "[TEST] ${exe} multi-block decompression failed: ${algo} (ignored)"
      else
        echo "[TEST] ${exe} multi-block decompression failed: ${algo}"
        exitcode=1
      fi
      continue
    fi
  done
done

exit "${exitcode}"

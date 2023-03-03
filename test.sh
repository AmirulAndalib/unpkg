#!/bin/sh

set -euf; unset -v IFS; export LC_ALL=C

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

    a="${test_dir}/single.${exe##*/}.raw.a"
    b="${test_dir}/single.${exe##*/}.raw.b"
    t="${test_dir}/single.${exe##*/}.${algo}"
    head -c 524288 </dev/zero >"${a}"
    head -c 524288 </dev/urandom >>"${a}"
    compression_tool -encode -A "${algo}" -b 1m <"${a}" >"${t}"
    if "./${exe}" <"${t}" >"${b}" && diff -q "${a}" "${b}"; then
      echo "[TEST] ${exe} single-block decompression succeeded: ${algo}"
      rm -- "${a}" "${b}" "${t}"
    else
      if [ pbzz:zlib = "${exe}:${algo}" ]; then
        echo "[TEST] ${exe} single-block decompression failed: ${algo} (ignored)"
      else
        echo "[TEST] ${exe} single-block decompression failed: ${algo}"
        exitcode=1
      fi
    fi

    raw="${test_dir}/multi.${algo}.raw"
    archive="${test_dir}/multi.${algo}.${exe##*/}"
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

if [ 0 -eq "${exitcode}" ]; then
  rm -R -- "${test_dir}"
fi

exit "${exitcode}"

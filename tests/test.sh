set -e

cd "$(dirname "$0")"

# TC="/home/karin0/lark/buaa/ct/sysy/code/TrivialCompiler/cmake-build-debug/TrivialCompiler"
proj=".."
MARS="../../mars.jar"

proj="$(realpath "$proj")"
if [ ! -d build ]; then
    mkdir build
fi
cd build
cmake "$proj" -DCMAKE_BUILD_TYPE=Release
make -j8
cd ..
PROG="build/syc"

test() {
    srcf=$1
    inf=$2
    ansf=$3
    echo test "$srcf" "$inf" "$ansf"
    if $PROG <"$srcf" >out.asm; then
        echo "$srcf" compiled
    else
        echo "$srcf" compiler re
        exit
    fi

    if [ "$inf" ]; then
        # if diff -b out.txt "$ansf"; then
        if [ -f "$inf" ]; then
            ./split <"$inf" >in.2.txt
            cp in.2.txt in.txt
            inf=in.txt
        else
            inf=/dev/null
        fi
        # if [ ! "$ansf" ]; then
            cat my.h "$srcf" | gcc -x c -O0 -o a.out -
            ./a.out <"$inf" >ans.txt
            ansf=ans.txt
        # fi
        if timeout 2 java -jar "$MARS" nc me mc Default out.asm <"$inf" >out.txt; then
            if diff -b out.txt "$ansf"; then
                echo "$srcf" ac
            else
                echo "$srcf" wa
                cp "$srcf" src.c
                cp "$ansf" ans.txt
                exit
            fi
        else
            echo "$srcf" re
            cp "$srcf" src.c
            cp "$ansf" ans.txt
            exit
        fi
    fi
}

# test src.c in.txt
# exit

for s in cases/hw1/out/testfile*; do
    dir=$(dirname "$s")
    base=$(basename "$s")
    test "$s" "$dir/${base/testfile/input}" "$dir/${base/testfile/output}"
done

# A, B, C done !!
# modified: C/7, B/忘了，A/8, B/18(a[-1]), B/19, B/1 (args eval order), A/3, A/26, A/19, A/13
for s in cases/20210926163332268/testfiles/*/testfile*; do
    dir=$(dirname "$s")
    base=$(basename "$s")
    test "$s" "$dir/${base/testfile/input}" "$dir/${base/testfile/output}"
done

# test ./cases/gram ./cases/gram.ans

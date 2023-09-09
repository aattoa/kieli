all: kieli

dependencies:
	tools/clone-deps.sh

update: dependencies
	tools/update-deps.sh

kieli: dependencies
	cmake -S . -B out/default
	cmake --build out/default -j 8

test: dependencies
	cmake -S . -B out/default -DKIELI_BUILD_TESTS=ON
	cmake --build out/default -j 8 --target all test

clean:
	rm -rf out

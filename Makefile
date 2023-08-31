all: kieli

dependencies:
	tools/clone-deps.sh

update: dependencies
	tools/update-deps.sh

configure: dependencies
	cmake -S . -B out/default

kieli: configure
	cmake --build out/default -j 8

test: kieli
	cmake --build out/default --target test

clean:
	rm -rf out dependencies

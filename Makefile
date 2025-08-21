IMAGE = x5-ble-build

build-image:
	docker build --platform=linux/arm64 --target=compile -t $(IMAGE) .

build: build-image
	docker create --platform=linux/arm64 --name tmp $(IMAGE)
	docker cp tmp:/out ./out
	docker rm -f tmp
	@echo "✅ build done. artifact: ./out/ble_min"

debug: build-image
	docker run --platform=linux/arm64 -it --rm $(IMAGE) bash

clean:
	rm -rf build out
	docker rmi -f $(IMAGE) || true

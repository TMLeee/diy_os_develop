all: ImageMaker Bin2C

ImageMaker:
	@echo 
	@echo ============= Build ImageMaker ===============
	@echo 
	
	make -C 00.ImageMaker

	@echo 
	@echo =============== Build Complete ===============
	@echo 
	
Bin2C:
	@echo 
	@echo ============= Build Bin2C ====================
	@echo 
	
	make -C 01.Bin2C
	
	@echo 
	@echo =============== Build Complete ===============
	@echo 
	
clean:
	make -C 00.ImageMaker clean
	make -C 01.Bin2C clean

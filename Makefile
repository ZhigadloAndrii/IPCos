.PHONY: clean All

All:
	@echo "----------Building project:[ File - Debug ]----------"
	@cd "File" && "$(MAKE)" -f  "File.mk"
clean:
	@echo "----------Cleaning project:[ File - Debug ]----------"
	@cd "File" && "$(MAKE)" -f  "File.mk" clean

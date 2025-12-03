CFLAGS=-pthread -D_POSIX_C_SOURCE=200809L -g -O0 -Iinclude
# Só ativa sanitizers se não estivermos no cygwin nem num vgbuild
ifneq ($(OS),Windows_NT)
	ifneq ($(DISABLE_SANS),1)
		CFLAGS +=  -fsanitize=address -fsanitize=undefined
	endif
endif
LFLAGS=
OUTPUT=program
LIBS=-lm 

# Configurações de dependências automáticas
DEPFLAGS=-MT $@ -MMD -MP -MF build/$*.Td

# Arquivos fonte
SOURCES=$(wildcard *.c) $(wildcard src/*.c)
OBJS:=$(patsubst %.c,build/%.o,$(SOURCES))

# Targets phony
.PHONY: all submission compile clean run vgbuild valgrind

# Cria diretórios de build
$(shell mkdir -p build build/src >/dev/null)

# ========== COMPILAÇÃO ==========

# Default: make == make all == make atc_simulator
all: $(OUTPUT)  # ← MUDEI AQUI: Agora depende do executável na raiz

# Compila para valgrind (desativa sanitizers)
vgbuild: clean
	$(MAKE) DISABLE_SANS=1 all
	echo -e "#!/bin/bash\nvalgrind --leak-check=full ./$(OUTPUT) \"\$$@\"" > run-valgrind.sh
	chmod +x run-valgrind.sh

# Regras de compilação
build/%.o : %.c build/%.d
	$(CC) -Wall -Werror -std=c11 $(CFLAGS) $(DEPFLAGS) -o $@ -c $<
	mv -f build/$*.Td build/$*.d && touch $@

build/src/%.o : src/%.c build/src/%.d
	$(CC) -Wall -Werror -std=c11 $(CFLAGS) $(DEPFLAGS) -o $@ -c $<
	mv -f build/$*.Td build/src/$*.d && touch $@

# ========== EXECUTÁVEL FINAL ==========

# Link final: cria executável NA RAIZ
$(OUTPUT): $(OBJS)  # ← ESTA É A REGRA PRINCIPAL AGORA
	$(CC) -Wall -Werror -std=c11 $(CFLAGS) $(LFLAGS) -o $@ $^ $(LIBS)

# ========== EXECUÇÃO ==========

# Executa com parâmetros padrão
run: $(OUTPUT)  # ← Agora depende do executável na raiz
	./$(OUTPUT) 5 8

# Executa com valgrind
valgrind: vgbuild
	valgrind --leak-check=full ./$(OUTPUT) 5 8

# ========== SUBMISSION (MOODLE) ==========

# Alias para compatibilidade com o makefile do professor
compile: all

# Prepara .tar.gz pra submissão no moodle (igual ao do professor)
submission: clean
	@rm -fr build $(OUTPUT)
	@SUBNAME=$$(basename "$$(pwd)"); \
		echo cd ..\; tar zcf "$$SUBNAME.tar.gz" "$$SUBNAME"
	@SUBNAME=$$(basename "$$(pwd)"); \
		cd ..; \
		rm -fr "$$SUBNAME.tar.gz"; \
		tar zcf "$$SUBNAME.tar.gz" "$$SUBNAME"
	@echo "Criado pacote \"$$(pwd).tar.gz\" para submissão"

# ========== LIMPEZA ==========

# Limpa tudo
clean:
	@rm -fr build $(OUTPUT) *.tar.gz run-valgrind.sh

# ========== DEPENDÊNCIAS AUTOMÁTICAS ==========

# Arquivos de dependência
build/%.d: ;
build/src/%.d: ;

.PRECIOUS: build/%.d build/src/%.d

include $(wildcard $(patsubst %,build/%.d,$(basename $(SOURCES))))
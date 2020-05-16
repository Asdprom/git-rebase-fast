// git-rebase-fast.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <git2.h>
#include <iostream>
#include <ctime>
#include <filesystem>

const git_signature* get_original_signature(git_oid oid, git_repository* repo)
{
	git_commit* base_commit = NULL;
	git_signature* signature = NULL;
	// получаем сам коммит, чтобы вытащить из него оригинального автора
	if (git_commit_lookup(&base_commit, repo, &oid)) {
		std::cout << "Failed to lookup commit by it's hash!" << std::endl;
		return NULL;
	}
	return git_commit_author(base_commit);
}


int main(int argc, char* argv[])
{
	if (argc < 3 or argc > 4) {
		std::cerr << "Usage: git-rebase-fast head %commit_number% \"New commit message\"" << std::endl;
		std::cerr << "Or: git-rebase-fast %commit_hash% \"New commit message\"" << std::endl;
		return 1;
	}

	/*
	* Если 1 параметр head (case-insensitive) то пытаемся следующий параметр и распарсить его как int, считываем новое сообщение
	* иначе считываем хеш, считываем новое сообщение.
	*/
	int commit_number = -1;
	char* new_message = NULL;
	char* commit_hash = NULL;
	if (_strcmpi(argv[1], "HEAD") == 0)
	{
		char* end;
		if ((commit_number = strtol(argv[2], &end, 10)) == 0)
		{
			new_message = argv[2];
		}
		else
		{
			if (argc == 4)
			{
				new_message = argv[3];
			}
			else
			{
				std::cout << "Wrong argument count." << std::endl;
			}
		}
	}
	else
	{
		commit_hash = argv[1];
		new_message = argv[2];
	}
	///=================================================
	git_repository* repo = NULL;
	git_libgit2_init();
	
	std::string str_path = std::filesystem::current_path().string();
	char* path = &str_path[0];	

	int error = git_repository_open(&repo, path);
	if (error < 0) {
		const git_error* e = git_error_last();
		printf("Error %d/%d: %s\n", error, e->klass, e->message);
		exit(error);
	}

	git_oid oid;
	git_reference* test = NULL;
	git_reference* master = NULL;
	git_annotated_commit* commit = NULL;

	git_rebase* rebase = NULL;
	git_rebase_options options = GIT_REBASE_OPTIONS_INIT;
	git_rebase_operation* operation = NULL;
	const git_signature* commit_author = NULL;

	if (commit_number < 0)
	{
		if (git_oid_fromstr(&oid, commit_hash) < 0)
		{
			std::cout << "Wrong commit id!" << std::endl;
			return 1;
		}
		//поиск по хешу, если он есть
		if (git_annotated_commit_lookup(&commit, repo, &oid))
		{
			std::cout << "Failed to lookup commit by it's hash!" << std::endl;
			return 1;
		}
	}
	else
	{
		git_revwalk* walker;

		git_revwalk_new(&walker, repo);
		git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL);
		git_revwalk_push_head(walker);

		int i = 0;
		// поиск коммита, с которого будем делать rebase по порядковому номеру
		// предполагаем, что искать надо начинать от текущего HEAD
		while (git_revwalk_next(&oid, walker) == 0 && i <= commit_number)
		{
			if (git_annotated_commit_lookup(&commit, repo, &oid)) {
				std::cout << "Failed to lookup commit by it's hash!" << std::endl;
				return 1;
			}

			i++;
		}
	}
	// получаем информацию об авторе оригинального коммита
	commit_author = get_original_signature(oid, repo);
	//******************************************************************

	if (git_rebase_init(&rebase, repo, NULL, commit, NULL, NULL) < 0)
	{
		const git_error* e = git_error_last();
		std::cout << "Error: " << e->klass << "\t" << e->message << std::endl;
		exit(error);
	}

	// операция начинается с нужного нам коммита
	int i = 0;
	while ((git_rebase_next(&operation, rebase)) == 0)
	{
		// поэтому сразу перезаписываем его комментарий
		if (i == 0)
			git_rebase_commit(&oid, rebase, commit_author, commit_author, NULL, new_message);
		// остальные коммитим как были
		else
			git_rebase_commit(&oid, rebase, NULL, commit_author, NULL, NULL);
		i++;
	}
	/************************************************
	* Идеи по поводу оптимизации:
	* 1. Основная идея заключается в том, чтобы, используя  git_repository_odb, git_odb_read, git_odb_write (т.е. работая напрямую с хранилищем) подменить коммиты
	* т.е. образно говоря
	*	прочитать коммит -> получить его родителей и дочерние -> создать новый коммит (git_commit_create) ->
	* -> каким то образом перенести данные в новый??? (главная проблема - каким то образом привязать изменения старого коммита к новому, вроде как git_commit_create этого не позволяет...) ->
	* -> прописать ему тех же родителей -> проставить дочерним коммитам родителем наш новый коммит
	* Что-то мне подсказывает что в лоб это так не сработает... но может быть. К сожалению, я начал пытаться сделать, но не хватило свободного времени проверить эту идею.
	* 
	*
	* 2. Альтернативная идея - напрямую влезть в файл коммита, который лежит .git/object/%первые_два_символа_хеша%/%остальные_символы_хеша%, разобравшивсь в его структуре
	* Аккуратно исправить только нужные поля, пересчитать хеш коммита, придется пересоздать папку и файл, 99% вероятность убитого репозитория. Мне кажется что это очень плохой,
	* если вообще возможный, вариант.
	*************************************************/
	git_rebase_finish(rebase, commit_author);

	git_annotated_commit_free(commit);
	git_reference_free(test);
	git_reference_free(master);
	git_rebase_free(rebase);
	git_repository_free(repo);
	return 0;
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file

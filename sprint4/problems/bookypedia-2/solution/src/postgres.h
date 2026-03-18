#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "author.h"
#include "book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_(connection) {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAllAuthors() override;
    std::optional<domain::Author> GetAuthorBy(const std::string& author_name) override;
    std::optional<domain::Author> GetAuthorBy(const domain::AuthorId& id) override;
    void Delete(const std::string& name) override;
    void UpdateName(const std::string& old_name, const std::string& new_name) override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_(connection) {
    }

    void Save(const domain::Book& book) override;
    std::vector<domain::Book> GetAllBooks() override;
    std::vector<domain::Book> GetBooksBy(const std::string& author_name) override;
    void Delete(const std::string& title) override;
    void DeleteById(const domain::BookId& id) override;
    std::vector<domain::Book> FindByTitle(const std::string& title) override;
    std::optional<domain::Book> FindById(const domain::BookId& id) override;
    void Update(const domain::BookId& id, const std::string& new_title,
                const std::optional<int>& new_year) override;
    void SaveTags(const domain::BookId& book_id, const std::vector<std::string>& tags) override;
    std::vector<std::string> GetTags(const domain::BookId& book_id) override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & {
        return authors_;
    }

    BookRepositoryImpl& GetBooks() & {
        return books_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres
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
    void Delete(const domain::AuthorId& id) override;
    void Update(const domain::Author& author) override;
    std::vector<domain::Author> GetAllAuthors() override;
    std::optional<domain::Author> GetAuthorByName(const std::string& name) override;
    std::optional<domain::Author> GetAuthorById(const domain::AuthorId& id) override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_(connection) {
    }

    void Save(const domain::Book& book) override;
    void Delete(const domain::BookId& id) override;
    void Update(const domain::Book& book) override;
    std::vector<domain::Book> GetAllBooks() override;
    std::vector<domain::Book> GetBooksByAuthor(const std::string& author_name) override;
    std::vector<domain::Book> GetBooksByTitle(const std::string& title) override;
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
#pragma once
#include <pqxx/connection>
#include <pqxx/transaction>

#include "author.h"
#include "book.h"
#include "unit_of_work.h"

namespace postgres {

class UnitOfWorkImpl : public domain::UnitOfWork {
public:
    explicit UnitOfWorkImpl(pqxx::connection& connection);
    
    void Commit() override;
    void Rollback() override;
    domain::AuthorRepository& Authors() override;
    domain::BookRepository& Books() override;

private:
    pqxx::work work_;
    class AuthorRepositoryImpl : public domain::AuthorRepository {
    public:
        explicit AuthorRepositoryImpl(pqxx::work& work);
        void Save(const domain::Author& author) override;
        std::vector<domain::Author> GetAllAuthors() override;
        std::optional<domain::Author> GetAuthorBy(const std::string& author_name) override;
        std::optional<domain::Author> GetAuthorBy(const domain::AuthorId& id) override;
        void Delete(const std::string& name) override;
        void UpdateName(const std::string& old_name, const std::string& new_name) override;

    private:
        pqxx::work& work_;
    };

    class BookRepositoryImpl : public domain::BookRepository {
    public:
        explicit BookRepositoryImpl(pqxx::work& work);
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
        pqxx::work& work_;
    };

    AuthorRepositoryImpl authors_{work_};
    BookRepositoryImpl books_{work_};
};

class UnitOfWorkFactoryImpl : public domain::UnitOfWorkFactory {
public:
    explicit UnitOfWorkFactoryImpl(pqxx::connection& connection);
    std::unique_ptr<domain::UnitOfWork> CreateUnitOfWork() override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    UnitOfWorkFactoryImpl& GetUnitOfWorkFactory() & {
        return factory_;
    }

private:
    pqxx::connection connection_;
    UnitOfWorkFactoryImpl factory_{connection_};
};

}  // namespace postgres
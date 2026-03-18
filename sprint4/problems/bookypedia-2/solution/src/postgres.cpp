#include "postgres.h"
#include "unit_of_work.h"

#include <string>
#include <sstream>
#include <pqxx/zview.hxx>
#include <pqxx/pqxx>
#include <memory>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// ==================== UnitOfWorkImpl ====================

UnitOfWorkImpl::UnitOfWorkImpl(pqxx::connection& connection)
    : work_(connection) {
}

void UnitOfWorkImpl::Commit() {
    work_.commit();
}

void UnitOfWorkImpl::Rollback() {
    work_.abort();
}

domain::AuthorRepository& UnitOfWorkImpl::Authors() {
    return authors_;
}

domain::BookRepository& UnitOfWorkImpl::Books() {
    return books_;
}

// ==================== AuthorRepositoryImpl ====================

UnitOfWorkImpl::AuthorRepositoryImpl::AuthorRepositoryImpl(pqxx::work& work)
    : work_(work) {
}

void UnitOfWorkImpl::AuthorRepositoryImpl::Save(const domain::Author& author) {
    work_.exec_params(R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
                      author.GetId().ToString(), author.GetName());
}

void UnitOfWorkImpl::AuthorRepositoryImpl::Delete(const std::string& name) {
    // Сначала удаляем теги книг автора
    work_.exec_params(R"(
        DELETE FROM book_tags 
        WHERE book_id IN (
            SELECT id FROM books 
            WHERE author_id = (SELECT id FROM authors WHERE name = $1)
        )
    )"_zv, name);
    
    // Затем удаляем книги автора
    work_.exec_params("DELETE FROM books WHERE author_id = (SELECT id FROM authors WHERE name = $1)"_zv, name);
    
    // Наконец удаляем автора
    work_.exec_params("DELETE FROM authors WHERE name = $1"_zv, name);
}

void UnitOfWorkImpl::AuthorRepositoryImpl::UpdateName(const std::string& old_name, const std::string& new_name) {
    work_.exec_params("UPDATE authors SET name=$1 WHERE name=$2"_zv, new_name, old_name);
}

std::vector<domain::Author> UnitOfWorkImpl::AuthorRepositoryImpl::GetAllAuthors() {
    std::vector<domain::Author> authors;
    auto query_text = "SELECT * FROM authors ORDER BY name ASC"_zv;
    for (auto [id, name] : work_.query<std::string, std::string>(query_text)) {
        authors.emplace_back(domain::AuthorId::FromString(id), name);
    }
    return authors;
}

std::optional<domain::Author> UnitOfWorkImpl::AuthorRepositoryImpl::GetAuthorBy(const std::string& author_name) {
    auto query_text = "SELECT * FROM authors WHERE name=" + work_.quote(author_name);
    auto tmp_author = work_.query01<std::string, std::string>(query_text);
    if (tmp_author) {
        auto [id, name] = *tmp_author;
        return domain::Author(domain::AuthorId::FromString(id), name);
    };
    return std::nullopt;
};

std::optional<domain::Author> UnitOfWorkImpl::AuthorRepositoryImpl::GetAuthorBy(const domain::AuthorId& id) {
    auto query_text = "SELECT * FROM authors WHERE id=" + work_.quote(id.ToString());
    auto tmp_author = work_.query01<std::string, std::string>(query_text);
    if (tmp_author) {
        auto [id_str, name] = *tmp_author;
        return domain::Author(domain::AuthorId::FromString(id_str), name);
    };
    return std::nullopt;
}

// ==================== BookRepositoryImpl ====================

UnitOfWorkImpl::BookRepositoryImpl::BookRepositoryImpl(pqxx::work& work)
    : work_(work) {
}

void UnitOfWorkImpl::BookRepositoryImpl::Save(const domain::Book& book) {
    work_.exec_params(R"(
INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)
ON CONFLICT (id) DO UPDATE SET author_id=$2, title=$3, publication_year=$4;
)"_zv,
                      book.GetId().ToString(),
                      book.GetAuthorId().ToString(),
                      book.GetTitle(),
                      book.GetPublicationYear());
}

void UnitOfWorkImpl::BookRepositoryImpl::Delete(const std::string& title) {
    // Сначала удаляем теги книги
    work_.exec_params(R"(
        DELETE FROM book_tags 
        WHERE book_id IN (
            SELECT id FROM books WHERE title = $1
        )
    )"_zv, title);
    
    // Затем удаляем книгу
    work_.exec_params("DELETE FROM books WHERE title = $1"_zv, title);
}

void UnitOfWorkImpl::BookRepositoryImpl::DeleteById(const domain::BookId& id) {
    work_.exec_params("DELETE FROM book_tags WHERE book_id = $1"_zv, id.ToString());
    work_.exec_params("DELETE FROM books WHERE id = $1"_zv, id.ToString());
}

std::vector<domain::Book> UnitOfWorkImpl::BookRepositoryImpl::FindByTitle(const std::string& title) {
    std::vector<domain::Book> books;
    auto query_text = "SELECT id, author_id, title, publication_year FROM books WHERE title=" 
        + work_.quote(title) 
        + " ORDER BY publication_year ASC, title ASC";
    for (auto [id, author_id, title_, year] : work_.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(
            domain::BookId::FromString(id),
            domain::AuthorId::FromString(author_id),
            title_,
            year
        );
    }
    return books;
}

std::optional<domain::Book> UnitOfWorkImpl::BookRepositoryImpl::FindById(const domain::BookId& id) {
    auto query_text = "SELECT id, author_id, title, publication_year FROM books WHERE id=" 
        + work_.quote(id.ToString());
    auto result = work_.query01<std::string, std::string, std::string, int>(query_text);
    if (result) {
        auto [id_str, author_id, title, year] = *result;
        return domain::Book(
            domain::BookId::FromString(id_str),
            domain::AuthorId::FromString(author_id),
            title,
            year
        );
    }
    return std::nullopt;
}

void UnitOfWorkImpl::BookRepositoryImpl::Update(const domain::BookId& id, const std::string& new_title,
                                                const std::optional<int>& new_year) {
    if (new_year.has_value()) {
        work_.exec_params("UPDATE books SET title=$1, publication_year=$2 WHERE id=$3",
                        new_title, *new_year, id.ToString());
    } else {
        work_.exec_params("UPDATE books SET title=$1 WHERE id=$2",
                        new_title, id.ToString());
    }
}

void UnitOfWorkImpl::BookRepositoryImpl::SaveTags(const domain::BookId& book_id, const std::vector<std::string>& tags) {
    // Удаляем старые теги
    work_.exec_params("DELETE FROM book_tags WHERE book_id=$1"_zv, book_id.ToString());
    
    // Добавляем новые теги
    for (const auto& tag : tags) {
        if (!tag.empty()) {
            work_.exec_params("INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)"_zv,
                            book_id.ToString(), tag);
        }
    }
}

std::vector<std::string> UnitOfWorkImpl::BookRepositoryImpl::GetTags(const domain::BookId& book_id) {
    std::vector<std::string> tags;
    auto query_text = "SELECT tag FROM book_tags WHERE book_id=" + work_.quote(book_id.ToString()) + " ORDER BY tag ASC";
    for (auto [tag] : work_.query<std::string>(query_text)) {
        tags.push_back(tag);
    }
    return tags;
}

std::vector<domain::Book> UnitOfWorkImpl::BookRepositoryImpl::GetAllBooks() {
    std::vector<domain::Book> books;
    auto query_text = "SELECT id, author_id, title, publication_year FROM books ORDER BY title ASC, author_id ASC, publication_year ASC"_zv;
    for (auto [id, author_id, title, year] : work_.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(
            domain::BookId::FromString(id),
            domain::AuthorId::FromString(author_id),
            title,
            year
        );
    }
    return books;
}

std::vector<domain::Book> UnitOfWorkImpl::BookRepositoryImpl::GetBooksBy(const std::string& author_name) {
    std::vector<domain::Book> books;
    auto query_text = "SELECT id, author_id, title, publication_year FROM books WHERE author_id=(SELECT id FROM authors WHERE name="
        + work_.quote(author_name)
        + " LIMIT 1) ORDER BY publication_year ASC, title ASC";
    for (auto [id, author_id, title, year] : work_.query<std::string, std::string, std::string, int>(query_text)) {
        books.emplace_back(
            domain::BookId::FromString(id),
            domain::AuthorId::FromString(author_id),
            title,
            year
        );
    }
    return books;
}

// ==================== UnitOfWorkFactoryImpl ====================

UnitOfWorkFactoryImpl::UnitOfWorkFactoryImpl(pqxx::connection& connection)
    : connection_(connection) {
}

std::unique_ptr<domain::UnitOfWork> UnitOfWorkFactoryImpl::CreateUnitOfWork() {
    return std::make_unique<UnitOfWorkImpl>(connection_);
}

// ==================== Database ====================

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);

    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL,
    title varchar(100) NOT NULL,
    publication_year int NOT NULL,
    FOREIGN KEY (author_id) REFERENCES authors(id) ON DELETE CASCADE
);
)"_zv);

    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);

    work.commit();
}

}  // namespace postgres
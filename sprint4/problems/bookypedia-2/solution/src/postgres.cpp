#include "postgres.h"

#include <string>
#include <pqxx/zview.hxx>
#include <pqxx/pqxx>
#include <algorithm>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

// AuthorRepositoryImpl
void AuthorRepositoryImpl::Save(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        "INSERT INTO authors (id, name) VALUES ($1, $2) "
        "ON CONFLICT (id) DO UPDATE SET name=$2",
        author.GetId().ToString(), author.GetName());
    work.commit();
}

void AuthorRepositoryImpl::Delete(const domain::AuthorId& id) {
    pqxx::work work{connection_};
    // Сначала удаляем все книги автора (теги удалятся каскадно)
    work.exec_params("DELETE FROM books WHERE author_id = $1", id.ToString());
    // Затем удаляем автора
    work.exec_params("DELETE FROM authors WHERE id = $1", id.ToString());
    work.commit();
}

void AuthorRepositoryImpl::Update(const domain::Author& author) {
    pqxx::work work{connection_};
    work.exec_params(
        "UPDATE authors SET name = $1 WHERE id = $2",
        author.GetName(), author.GetId().ToString());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAllAuthors() {
    std::vector<domain::Author> authors;
    pqxx::read_transaction read{connection_};
    auto result = read.exec("SELECT id, name FROM authors ORDER BY name");
    for (auto row : result) {
        authors.emplace_back(
            domain::AuthorId::FromString(row[0].as<std::string>()), 
            row[1].as<std::string>()
        );
    }
    return authors;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorByName(const std::string& name) {
    pqxx::read_transaction read{connection_};
    auto result = read.exec_params("SELECT id, name FROM authors WHERE name = $1", name);
    if (!result.empty()) {
        auto row = result[0];
        return domain::Author(
            domain::AuthorId::FromString(row[0].as<std::string>()), 
            row[1].as<std::string>()
        );
    }
    return std::nullopt;
}

std::optional<domain::Author> AuthorRepositoryImpl::GetAuthorById(const domain::AuthorId& id) {
    pqxx::read_transaction read{connection_};
    auto result = read.exec_params("SELECT id, name FROM authors WHERE id = $1", id.ToString());
    if (!result.empty()) {
        auto row = result[0];
        return domain::Author(
            domain::AuthorId::FromString(row[0].as<std::string>()), 
            row[1].as<std::string>()
        );
    }
    return std::nullopt;
}

// BookRepositoryImpl
void BookRepositoryImpl::Save(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        "INSERT INTO books (id, author_id, title, publication_year) VALUES ($1, $2, $3, $4)",
        book.GetId().ToString(),
        book.GetAuthorId().ToString(),
        book.GetTitle(),
        book.GetPublicationYear());
    work.commit();
}

void BookRepositoryImpl::Delete(const domain::BookId& id) {
    pqxx::work work{connection_};
    // Сначала удаляем теги книги
    work.exec_params("DELETE FROM book_tags WHERE book_id = $1", id.ToString());
    // Затем удаляем книгу
    work.exec_params("DELETE FROM books WHERE id = $1", id.ToString());
    work.commit();
}

void BookRepositoryImpl::Update(const domain::Book& book) {
    pqxx::work work{connection_};
    work.exec_params(
        "UPDATE books SET title = $1, publication_year = $2 WHERE id = $3",
        book.GetTitle(), book.GetPublicationYear(), book.GetId().ToString());
    work.commit();
}

std::vector<domain::Book> BookRepositoryImpl::GetAllBooks() {
    std::vector<domain::Book> books;
    pqxx::read_transaction read{connection_};
    auto result = read.exec("SELECT id, author_id, title, publication_year FROM books ORDER BY title");
    for (auto row : result) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()),
            row[2].as<std::string>(),
            row[3].as<int>()
        );
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksByAuthor(const std::string& author_name) {
    std::vector<domain::Book> books;
    pqxx::read_transaction read{connection_};
    auto result = read.exec_params(
        "SELECT b.id, b.author_id, b.title, b.publication_year "
        "FROM books b "
        "JOIN authors a ON a.id = b.author_id "
        "WHERE a.name = $1 "
        "ORDER BY b.publication_year, b.title",
        author_name);
    
    for (auto row : result) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()),
            row[2].as<std::string>(),
            row[3].as<int>()
        );
    }
    return books;
}

std::vector<domain::Book> BookRepositoryImpl::GetBooksByTitle(const std::string& title) {
    std::vector<domain::Book> books;
    pqxx::read_transaction read{connection_};
    auto result = read.exec_params(
        "SELECT id, author_id, title, publication_year FROM books WHERE title = $1",
        title);
    
    for (auto row : result) {
        books.emplace_back(
            domain::BookId::FromString(row[0].as<std::string>()),
            domain::AuthorId::FromString(row[1].as<std::string>()),
            row[2].as<std::string>(),
            row[3].as<int>()
        );
    }
    return books;
}

void BookRepositoryImpl::SaveTags(const domain::BookId& book_id, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    
    // Удаляем старые теги
    work.exec_params("DELETE FROM book_tags WHERE book_id = $1", book_id.ToString());
    
    // Добавляем новые теги
    for(const auto& tag : tags) {
        if(!tag.empty()) {
            work.exec_params(
                "INSERT INTO book_tags (book_id, tag) VALUES ($1, $2)",
                book_id.ToString(), tag);
        }
    }
    
    work.commit();
}

std::vector<std::string> BookRepositoryImpl::GetTags(const domain::BookId& book_id) {
    std::vector<std::string> tags;
    pqxx::read_transaction read{connection_};
    auto result = read.exec_params(
        "SELECT tag FROM book_tags WHERE book_id = $1 ORDER BY tag",
        book_id.ToString());
    
    for (auto row : result) {
        tags.push_back(row[0].as<std::string>());
    }
    
    return tags;
}

// Database
Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    
    // Создаем таблицы если их нет
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);

    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title varchar(100) NOT NULL,
    publication_year int NOT NULL
);
)"_zv);

    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    PRIMARY KEY (book_id, tag)
);
)"_zv);

    work.commit();
}

}  // namespace postgres
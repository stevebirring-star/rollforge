#include "library/LibraryDb.h"

#include "library/sqlite/sqlite3.h"

namespace rollforge
{

LibraryDb::~LibraryDb()
{
    close();
}

bool LibraryDb::openAt (const char* filename)
{
    close();
    if (sqlite3_open (filename, &db) != SQLITE_OK)
    {
        close();
        return false;
    }
    return createSchema();
}

bool LibraryDb::open (const juce::File& dbFile)
{
    return openAt (dbFile.getFullPathName().toRawUTF8());
}

bool LibraryDb::openInMemory()
{
    return openAt (":memory:");
}

void LibraryDb::close()
{
    if (db != nullptr)
    {
        sqlite3_close (db);
        db = nullptr;
    }
}

bool LibraryDb::createSchema()
{
    if (db == nullptr)
        return false;

    const char* sql =
        "CREATE TABLE IF NOT EXISTS samples ("
        " path TEXT PRIMARY KEY, name TEXT, duration REAL, rms REAL, zcr REAL,"
        " decay REAL, onsets INTEGER, category INTEGER, confidence REAL, favourite INTEGER);"
        "CREATE TABLE IF NOT EXISTS category_overrides ("
        " path TEXT PRIMARY KEY, category INTEGER NOT NULL);";

    char* err = nullptr;
    if (sqlite3_exec (db, sql, nullptr, nullptr, &err) != SQLITE_OK)
    {
        if (err != nullptr) sqlite3_free (err);
        return false;
    }
    return true;
}

bool LibraryDb::upsert (const LibraryEntry& e)
{
    if (db == nullptr)
        return false;

    const char* sql =
        "INSERT INTO samples (path,name,duration,rms,zcr,decay,onsets,category,confidence,favourite)"
        " VALUES (?,?,?,?,?,?,?,?,?,?)"
        " ON CONFLICT(path) DO UPDATE SET name=excluded.name, duration=excluded.duration,"
        " rms=excluded.rms, zcr=excluded.zcr, decay=excluded.decay, onsets=excluded.onsets,"
        " category=excluded.category, confidence=excluded.confidence, favourite=excluded.favourite;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text   (stmt, 1, e.path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text   (stmt, 2, e.name.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double (stmt, 3, e.durationSeconds);
    sqlite3_bind_double (stmt, 4, e.rms);
    sqlite3_bind_double (stmt, 5, e.zcr);
    sqlite3_bind_double (stmt, 6, e.decay);
    sqlite3_bind_int    (stmt, 7, e.onsetCount);
    sqlite3_bind_int    (stmt, 8, (int) e.category);
    sqlite3_bind_double (stmt, 9, e.confidence);
    sqlite3_bind_int    (stmt, 10, e.favourite ? 1 : 0);

    const bool ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

bool LibraryDb::setFavourite (const juce::String& path, bool favourite)
{
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "UPDATE samples SET favourite=? WHERE path=?;", -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_int  (stmt, 1, favourite ? 1 : 0);
    sqlite3_bind_text (stmt, 2, path.toRawUTF8(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

bool LibraryDb::setCategoryOverride (const juce::String& path, SoundCategory category)
{
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "INSERT INTO category_overrides (path,category) VALUES (?,?)"
                                " ON CONFLICT(path) DO UPDATE SET category=excluded.category;",
                            -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text (stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int  (stmt, 2, (int) category);

    const bool ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

bool LibraryDb::clearCategoryOverride (const juce::String& path)
{
    if (db == nullptr)
        return false;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "DELETE FROM category_overrides WHERE path=?;", -1, &stmt, nullptr) != SQLITE_OK)
        return false;

    sqlite3_bind_text (stmt, 1, path.toRawUTF8(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step (stmt) == SQLITE_DONE;
    sqlite3_finalize (stmt);
    return ok;
}

int LibraryDb::count() const
{
    if (db == nullptr)
        return 0;

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, "SELECT COUNT(*) FROM samples;", -1, &stmt, nullptr) != SQLITE_OK)
        return 0;

    int n = 0;
    if (sqlite3_step (stmt) == SQLITE_ROW)
        n = sqlite3_column_int (stmt, 0);
    sqlite3_finalize (stmt);
    return n;
}

std::vector<LibraryEntry> LibraryDb::queryWhere (const juce::String& whereClause) const
{
    std::vector<LibraryEntry> out;
    if (db == nullptr)
        return out;

    // The category column is the OVERRIDE if one exists, else the auto-detected one,
    // so a manual re-tag is honoured by every query without touching the samples row.
    const juce::String sql =
        "SELECT s.path,s.name,s.duration,s.rms,s.zcr,s.decay,s.onsets,"
        " COALESCE(o.category,s.category),s.confidence,s.favourite"
        " FROM samples s LEFT JOIN category_overrides o ON s.path=o.path "
        + whereClause + ";";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2 (db, sql.toRawUTF8(), -1, &stmt, nullptr) != SQLITE_OK)
        return out;

    while (sqlite3_step (stmt) == SQLITE_ROW)
    {
        LibraryEntry e;
        e.path            = juce::String::fromUTF8 (reinterpret_cast<const char*> (sqlite3_column_text (stmt, 0)));
        e.name            = juce::String::fromUTF8 (reinterpret_cast<const char*> (sqlite3_column_text (stmt, 1)));
        e.durationSeconds = (float) sqlite3_column_double (stmt, 2);
        e.rms             = (float) sqlite3_column_double (stmt, 3);
        e.zcr             = (float) sqlite3_column_double (stmt, 4);
        e.decay           = (float) sqlite3_column_double (stmt, 5);
        e.onsetCount      = sqlite3_column_int (stmt, 6);
        e.category        = (SoundCategory) sqlite3_column_int (stmt, 7);
        e.confidence      = (float) sqlite3_column_double (stmt, 8);
        e.favourite       = sqlite3_column_int (stmt, 9) != 0;
        out.push_back (e);
    }
    sqlite3_finalize (stmt);
    return out;
}

std::vector<LibraryEntry> LibraryDb::all() const
{
    return queryWhere ("ORDER BY s.name");
}

std::vector<LibraryEntry> LibraryDb::byCategory (SoundCategory category) const
{
    // Filter on the effective (override-aware) category so a re-tag moves the sample.
    return queryWhere ("WHERE COALESCE(o.category,s.category)=" + juce::String ((int) category)
                       + " ORDER BY s.name");
}

std::vector<LibraryEntry> LibraryDb::favourites() const
{
    return queryWhere ("WHERE s.favourite=1 ORDER BY s.name");
}

} // namespace rollforge

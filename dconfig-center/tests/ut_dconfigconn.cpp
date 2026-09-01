// SPDX-FileCopyrightText: 2021 - 2026 Uniontech Software Technology Co.,Ltd.
//
// SPDX-License-Identifier: LGPL-3.0-or-later

#include <QBuffer>
#include <QFile>
#include <QLocale>
#include <QSignalSpy>
#include <QDir>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>

#include <gtest/gtest.h>

#include "dconfigresource.h"
#include "dconfigconn.h"
#include "dconfigrefmanager.h"
#include <memory>
#include "test_helper.hpp"

static const QString LocalPrefix = QDir::tempPath() + "/dde-app-services-test-" +
    QString::number(QCoreApplication::applicationPid()) + "/";
static constexpr char const *APP_ID = "org.foo.appid";
static constexpr char const *FILE_NAME = "example";

static QString configPath()
{
    const QString metaPath = QString("%1/usr/share/dsg/configs/%2").arg(LocalPrefix, APP_ID);
    return QString("%1/%2.json").arg(metaPath, FILE_NAME);
}
static QString noAppIdConfigPath()
{
    const QString metaPath = QString("%1/usr/share/dsg/configs/").arg(LocalPrefix);

    return QString("%1/%2.json").arg(metaPath, FILE_NAME);
}

static EnvGuard dsgDataDir;
class ut_DConfigResource : public testing::Test
{
protected:
    static void SetUpTestCase() {
        auto path = configPath();
        if (!QFile::exists(path)) {
            QDir("").mkpath(QFileInfo(path).path());
        }

        ASSERT_TRUE(QFile::copy(":/config/example.json", path));
        ASSERT_TRUE(QFile::copy(":/config/example.json", noAppIdConfigPath()));
        qputenv("DSG_CONFIG_CONNECTION_DISABLE_DBUS", "true");
        qputenv("STATE_DIRECTORY", LocalPrefix.toLocal8Bit());
        dsgDataDir.set("DSG_DATA_DIRS", "/usr/share/dsg");
    }
    static void TearDownTestCase() {
        QFile::remove(configPath());
        QFile::remove(noAppIdConfigPath());
        QDir(LocalPrefix).removeRecursively();
        qunsetenv("DSG_CONFIG_CONNECTION_DISABLE_DBUS");
        qunsetenv("STATE_DIRECTORY");
        dsgDataDir.restore();
    }
    virtual void SetUp() override {
        resource.reset(new DSGConfigResource(FILE_NAME, "", LocalPrefix));
    }
    virtual void TearDown() override {

    }

    QScopedPointer<DSGConfigResource> resource;
};

TEST_F(ut_DConfigResource, load) {

    ASSERT_TRUE(resource->load(APP_ID));
    ASSERT_TRUE(resource->load(VirtualInterAppId));
}
TEST_F(ut_DConfigResource, load_fail) {

    DSGConfigResource resource2("example_notexist", "");
    ASSERT_FALSE(resource2.load(APP_ID));
}
TEST_F(ut_DConfigResource, createConn) {

    resource->load(APP_ID);
    ASSERT_TRUE(resource->createConn(APP_ID, TestUid));

    resource->load(VirtualInterAppId);
    ASSERT_TRUE(resource->createConn(VirtualInterAppId, TestUid));
}
TEST_F(ut_DConfigResource, getConn) {

    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    ASSERT_TRUE(resource->getConn(APP_ID, TestUid));

    resource->load(VirtualInterAppId);
    resource->createConn(VirtualInterAppId, TestUid);
    ASSERT_TRUE(resource->getConn(VirtualInterAppId, TestUid));

    ASSERT_EQ(resource->connSize(), 2);
}
TEST_F(ut_DConfigResource, fallbackToGenericConfig) {

    resource->load(APP_ID);
    ASSERT_TRUE(resource->fallbackToGenericConfig());
}

class ut_DConfigConn : public testing::Test
{
protected:
    static void SetUpTestCase() {
        auto path = configPath();
        if (!QFile::exists(path)) {
            QDir("").mkpath(QFileInfo(path).path());
        }

        ASSERT_TRUE(QFile::copy(":/config/example.json", path));
        ASSERT_TRUE(QFile::copy(":/config/example.json", noAppIdConfigPath()));
        qputenv("DSG_CONFIG_CONNECTION_DISABLE_DBUS", "true");
        qputenv("STATE_DIRECTORY", LocalPrefix.toLocal8Bit());
        dsgDataDir.set("DSG_DATA_DIRS", "/usr/share/dsg");
    }
    static void TearDownTestCase() {
        QFile::remove(configPath());
        QFile::remove(noAppIdConfigPath());
        QDir(LocalPrefix).removeRecursively();
        qunsetenv("DSG_CONFIG_CONNECTION_DISABLE_DBUS");
        qunsetenv("STATE_DIRECTORY");
        dsgDataDir.restore();
    }
    virtual void SetUp() override {
        resource.reset(new DSGConfigResource("example", "", LocalPrefix));
        resource->load(APP_ID);
        conn = resource->createConn(APP_ID, TestUid);
        ASSERT_TRUE(conn);
    }
    virtual void TearDown() override {

    }
    DSGConfigConn* conn;
    QScopedPointer<DSGConfigResource> resource;
};

TEST_F(ut_DConfigConn, description_name) {
    ASSERT_EQ(conn->description("canExit", ""), "我是描述");
    ASSERT_EQ(conn->description("canExit", "en_US"), "I am description");
    ASSERT_EQ(conn->name("canExit", ""), "I am name");
    ASSERT_EQ(conn->name("canExit", "zh_CN"), QString("我是名字"));
    ASSERT_EQ(conn->name("canExit", QLocale(QLocale::Japanese).name()), "");
    ASSERT_EQ(conn->flags("canExit"), 0);
}

TEST_F(ut_DConfigConn, value) {
    conn->setValue("canExit", QDBusVariant{true});
    ASSERT_EQ(conn->value("canExit").variant(), true);
    const QStringList array{"value1", "value2"};
    QVariantMap map;
    map.insert("key1", "value1");
    map.insert("key2", "value2");
    ASSERT_EQ(conn->value("array").variant().toStringList(), array);
    ASSERT_EQ(conn->value("map").variant().toMap(), map);

    // array_map is an ARRAY containing one MAP: [{"key1":"value1","key2":"value2"}]
    QVariantList arrayMap = conn->value("array_map").variant().toList();
    ASSERT_EQ(arrayMap.size(), 1);
    QVariantMap mapInArray = arrayMap.first().toMap();
    ASSERT_EQ(mapInArray.value("key1").toString(), "value1");
    ASSERT_EQ(mapInArray.value("key2").toString(), "value2");
}

TEST_F(ut_DConfigConn, value_default) {
    ASSERT_EQ(conn->value("canExit").variant(), true);
}

TEST_F(ut_DConfigConn, setValue_andGetValue) {
    conn->setValue("canExit", QDBusVariant{false});
    ASSERT_EQ(conn->value("canExit").variant(), false);
    conn->setValue("canExit", QDBusVariant{true});
    ASSERT_EQ(conn->value("canExit").variant(), true);
}

TEST_F(ut_DConfigConn, permissions) {
    ASSERT_EQ(conn->permissions("canExit"), "readwrite");
    ASSERT_EQ(conn->permissions("key2"), "readwrite");
}

TEST_F(ut_DConfigConn, visibility) {
    // canExit has "visibility": "private" in example.json
    ASSERT_EQ(conn->visibility("canExit"), "private");
    ASSERT_EQ(conn->visibility("key2"), "public");
}

TEST_F(ut_DConfigConn, key) {
    ASSERT_FALSE(conn->key().isEmpty());
}

TEST_F(ut_DConfigConn, resourceKey) {
    ASSERT_FALSE(getResourceKey(conn->key()).isEmpty());
}

TEST_F(ut_DConfigConn, appId) {
    // When calledFromDBus() is false (test env with DSG_CONFIG_CONNECTION_DISABLE_DBUS=true),
    // getAppid() returns "testappid"
    ASSERT_EQ(conn->getAppid(), QString("testappid"));
}

TEST_F(ut_DConfigConn, uid) {
    ASSERT_EQ(getConnectionKey(conn->key()), TestUid);
}

TEST_F(ut_DConfigConn, file) {
    ASSERT_NE(conn->file(), nullptr);
}

TEST_F(ut_DConfigConn, meta) {
    ASSERT_NE(conn->meta(), nullptr);
}

TEST_F(ut_DConfigConn, setResource_updatesResource) {
    auto oldFile = conn->file();
    ASSERT_NE(oldFile, nullptr);

    auto resource2 = std::make_unique<DSGConfigResource>(FILE_NAME, "", LocalPrefix);
    resource2->load(APP_ID);
    conn->setResource(resource2.get());

    auto newFile = conn->file();
    ASSERT_NE(newFile, nullptr);
    ASSERT_NE(newFile, oldFile);

    // Restore original resource pointer to avoid use-after-free on TearDown
    conn->setResource(resource.data());
}

// Coverage-9: specificAppConns
TEST_F(ut_DConfigResource, specificAppConns_returnsOnlyAppConns) {
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    auto conns = resource->specificAppConns();
    ASSERT_FALSE(conns.isEmpty());
}

// Coverage-9: cacheExist
TEST_F(ut_DConfigResource, cacheExist_true) {
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    auto resourceKey = getResourceKey(APP_ID, resource->key());
    ASSERT_TRUE(resource->cacheExist(resourceKey));
}

TEST_F(ut_DConfigResource, cacheExist_false) {
    ASSERT_FALSE(resource->cacheExist("/nonexistent/resource"));
}

// Coverage-9: cachesOfTheResource
TEST_F(ut_DConfigResource, cachesOfTheResource_returnsCaches) {
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    auto resourceKey = getResourceKey(APP_ID, resource->key());
    auto caches = resource->cachesOfTheResource(resourceKey);
    ASSERT_FALSE(caches.isEmpty());
}

TEST_F(ut_DConfigResource, cachesOfTheResource_emptyForNonExistent) {
    auto caches = resource->cachesOfTheResource("/nonexistent/resource");
    ASSERT_TRUE(caches.isEmpty());
}

// Coverage-9: connsOfTheResource
TEST_F(ut_DConfigResource, connsOfTheResource_returnsConns) {
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    auto resourceKey = getResourceKey(APP_ID, resource->key());
    auto conns = resource->connsOfTheResource(resourceKey);
    ASSERT_EQ(conns.size(), 1);
}

TEST_F(ut_DConfigResource, connsOfTheResource_emptyForNonExistent) {
    auto conns = resource->connsOfTheResource("/nonexistent/resource");
    ASSERT_TRUE(conns.isEmpty());
}

// Coverage-9: repareCache
TEST_F(ut_DConfigResource, repareCache_sameMeta_preservesKeys) {
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    auto connKey = resource->getConnKey(APP_ID, TestUid);
    auto cache = resource->getCache(connKey);
    ASSERT_NE(cache, nullptr);
    auto file = resource->getFile(getResourceKey(APP_ID, resource->key()));
    ASSERT_NE(file, nullptr);
    auto meta = file->meta();
    auto keysBefore = cache->keyList();
    resource->repareCache(cache, meta, meta);
    auto keysAfter = cache->keyList();
    ASSERT_EQ(keysAfter.size(), keysBefore.size());
}

TEST_F(ut_DConfigResource, getConnectionsByUid) {
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    resource->load(VirtualInterAppId);
    resource->createConn(VirtualInterAppId, TestUid);

    auto conns = resource->getConnectionsByUid(TestUid);
    ASSERT_EQ(conns.size(), 2);
}

TEST_F(ut_DConfigResource, getConnectionsByUid_noMatch) {
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);

    auto conns = resource->getConnectionsByUid(99999);
    ASSERT_TRUE(conns.isEmpty());
}

TEST_F(ut_DConfigResource, isEmptyConn) {
    ASSERT_TRUE(resource->isEmptyConn());
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    ASSERT_FALSE(resource->isEmptyConn());
}

TEST_F(ut_DConfigResource, noAppidCache) {
    resource->load(APP_ID);
    resource->load(VirtualInterAppId);
    resource->createConn(VirtualInterAppId, TestUid);
    auto cache = resource->noAppidCache(TestUid);
    ASSERT_NE(cache, nullptr);
}

TEST_F(ut_DConfigResource, noAppidFile) {
    resource->load(APP_ID);
    resource->load(VirtualInterAppId);
    resource->createConn(VirtualInterAppId, TestUid);
    auto file = resource->noAppidFile();
    ASSERT_NE(file, nullptr);
}

TEST_F(ut_DConfigResource, removeConn) {
    resource->load(APP_ID);
    auto conn = resource->createConn(APP_ID, TestUid);
    ASSERT_EQ(resource->connSize(), 1);
    resource->removeConn(conn->key());
    ASSERT_EQ(resource->connSize(), 0);
}

TEST_F(ut_DConfigResource, save_preservesData) {
    resource->load(APP_ID);
    auto conn = resource->createConn(APP_ID, TestUid);
    ASSERT_TRUE(conn);
    conn->setValue("canExit", QDBusVariant{false});
    ASSERT_EQ(conn->value("canExit").variant(), false);
    resource->save();
    ASSERT_EQ(conn->value("canExit").variant(), false);
    ASSERT_EQ(resource->connSize(), 1);
    // Assert the user cache file was actually written to disk.
    QString userCachePath = configPrefixPath() + "/" + QString::number(TestUid) +
        "/" + APP_ID + "/" + FILE_NAME + ".json";
    ASSERT_TRUE(QFile::exists(userCachePath));
}

TEST_F(ut_DConfigResource, save_withAppid_preservesData) {
    resource->load(APP_ID);
    auto conn = resource->createConn(APP_ID, TestUid);
    ASSERT_TRUE(conn);
    conn->setValue("canExit", QDBusVariant{true});
    ASSERT_EQ(conn->value("canExit").variant(), true);
    resource->save(APP_ID);
    ASSERT_EQ(conn->value("canExit").variant(), true);
    ASSERT_EQ(resource->connSize(), 1);
    // Assert the user cache file was actually written to disk.
    QString userCachePath = configPrefixPath() + "/" + QString::number(TestUid) +
        "/" + APP_ID + "/" + FILE_NAME + ".json";
    ASSERT_TRUE(QFile::exists(userCachePath));
}

TEST_F(ut_DConfigResource, reparse_existingResource) {
    EnvGuard localDsgDir;
    localDsgDir.set("DSG_DATA_DIRS", (LocalPrefix + "/usr/share/dsg").toLocal8Bit());
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    ASSERT_TRUE(resource->reparse(APP_ID));
}

TEST_F(ut_DConfigResource, reparse_metaLoadFailure_returnsFalse) {
    EnvGuard localDsgDir;
    localDsgDir.set("DSG_DATA_DIRS", (LocalPrefix + "/usr/share/dsg").toLocal8Bit());
    resource->load(APP_ID);
    resource->createConn(APP_ID, TestUid);
    QString appMetaPath = configPath();
    QString genericMetaPath = noAppIdConfigPath();
    MetaFileGuard appGuard(appMetaPath);
    MetaFileGuard genericGuard(genericMetaPath);
    QFile::remove(appMetaPath);
    QFile::remove(genericMetaPath);
    EXPECT_FALSE(resource->reparse(APP_ID));
}

TEST_F(ut_DConfigResource, doGlobalValueChanged_withoutSyncCache_noCrash) {
    resource->load(APP_ID);
    auto conn = resource->createConn(APP_ID, TestUid);
    ASSERT_TRUE(conn);
    QSignalSpy spy(conn, &DSGConfigConn::valueChanged);
    auto resourceKey = getResourceKey(APP_ID, resource->key());
    resource->doGlobalValueChanged("array", resourceKey);
    ASSERT_EQ(spy.count(), 1);
}

TEST_F(ut_DConfigResource, reparse_withRemovedKey_removesFromCache) {
    EnvGuard localDsgDir;
    localDsgDir.set("DSG_DATA_DIRS", (LocalPrefix + "/usr/share/dsg").toLocal8Bit());
    resource->load(APP_ID);
    auto conn = resource->createConn(APP_ID, TestUid);
    ASSERT_TRUE(conn);
    conn->setValue("canExit", QDBusVariant{false});
    ASSERT_EQ(conn->value("canExit").variant(), false);
    QString metaPath = configPath();
    {
        MetaFileGuard guard(metaPath);
        QFile file(metaPath);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        QJsonObject root = doc.object();
        QJsonObject contents = root.value("contents").toObject();
        contents.remove("canExit");
        root["contents"] = contents;
        doc.setObject(root);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(doc.toJson());
        file.close();
        ASSERT_TRUE(resource->reparse(APP_ID));
        EXPECT_FALSE(conn->value("canExit").variant().isValid());
    }
}

TEST_F(ut_DConfigResource, reparse_withPermissionChange_removesFromCache) {
    EnvGuard localDsgDir;
    localDsgDir.set("DSG_DATA_DIRS", (LocalPrefix + "/usr/share/dsg").toLocal8Bit());
    resource->load(APP_ID);
    auto conn = resource->createConn(APP_ID, TestUid);
    ASSERT_TRUE(conn);
    conn->setValue("canExit", QDBusVariant{false});
    ASSERT_EQ(conn->value("canExit").variant(), false);
    QString metaPath = configPath();
    {
        MetaFileGuard guard(metaPath);
        QFile file(metaPath);
        ASSERT_TRUE(file.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        file.close();
        QJsonObject root = doc.object();
        QJsonObject contents = root.value("contents").toObject();
        QJsonObject canExit = contents.value("canExit").toObject();
        canExit["permissions"] = "readonly";
        contents["canExit"] = canExit;
        root["contents"] = contents;
        doc.setObject(root);
        ASSERT_TRUE(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.write(doc.toJson());
        file.close();
        ASSERT_TRUE(resource->reparse(APP_ID));
        conn->setValue("canExit", QDBusVariant{false});
        EXPECT_EQ(conn->value("canExit").variant(), true);
    }
}

TEST_F(ut_DConfigResource, getConn_byKey) {
    resource->load(APP_ID);
    auto conn = resource->createConn(APP_ID, TestUid);
    ASSERT_EQ(resource->getConn(conn->key()), conn);
}

TEST_F(ut_DConfigResource, getConn_nonexistent) {
    resource->load(APP_ID);
    ASSERT_EQ(resource->getConn("nonexistent_key"), nullptr);
}

TEST_F(ut_DConfigResource, getFile) {
    resource->load(APP_ID);
    auto resourceKey = getResourceKey(APP_ID, resource->key());
    auto file = resource->getFile(resourceKey);
    ASSERT_NE(file, nullptr);
}

TEST_F(ut_DConfigResource, getFile_nonexistent) {
    auto file = resource->getFile("/nonexistent/resource");
    ASSERT_EQ(file, nullptr);
}

TEST_F(ut_DConfigResource, getCache_nonexistent) {
    auto cache = resource->getCache("/nonexistent/conn");
    ASSERT_EQ(cache, nullptr);
}

TEST_F(ut_DConfigConn, doSyncConfigCache_savesCache) {
    conn->setValue("canExit", QDBusVariant{false});
    ASSERT_EQ(conn->value("canExit").variant(), false);
    ConfigCacheKey userKey = ConfigSyncRequestCache::userKey(conn->key());
    ConfigSyncRequestCache syncCache(resource.data());
    resource->setSyncRequestCache(&syncCache);
    resource->doSyncConfigCache(userKey);
    auto connKey = resource->getConnKey(APP_ID, TestUid);
    auto cache = resource->getCache(connKey);
    ASSERT_NE(cache, nullptr);
    ASSERT_EQ(conn->value("canExit").variant(), false);
}



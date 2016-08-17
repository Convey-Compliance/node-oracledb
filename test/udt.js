var oracledb = require('oracledb');
var should   = require('should');
var async    = require('async');
var dbConfig = require('./dbconfig.js');

describe('67 udt.js', function() {

  var connection = null;
  var createTables =
      "BEGIN \
          DECLARE \
              e_table_missing EXCEPTION; \
              PRAGMA EXCEPTION_INIT(e_table_missing, -00942); \
          BEGIN \
              EXECUTE IMMEDIATE ('DROP TABLE udt_table'); \
          EXCEPTION \
              WHEN e_table_missing \
              THEN NULL; \
          END; \
          DECLARE \
              e_table_missing EXCEPTION; \
              PRAGMA EXCEPTION_INIT(e_table_missing, -00942); \
          BEGIN \
              EXECUTE IMMEDIATE ('DROP TABLE udt_nested'); \
          EXCEPTION \
              WHEN e_table_missing \
              THEN NULL; \
          END; \
          DECLARE \
              e_table_missing EXCEPTION; \
              PRAGMA EXCEPTION_INIT(e_table_missing, -00942); \
          BEGIN \
              EXECUTE IMMEDIATE ('DROP TABLE udt'); \
          EXCEPTION \
              WHEN e_table_missing \
              THEN NULL; \
          END; \
          BEGIN \
              EXECUTE IMMEDIATE ('DROP TYPE str_kvp_table'); \
          EXCEPTION \
              WHEN OTHERS \
              THEN NULL; \
          END; \
          BEGIN \
              EXECUTE IMMEDIATE ('DROP TYPE str_kvp_nested'); \
          EXCEPTION \
              WHEN OTHERS \
              THEN NULL; \
          END; \
          BEGIN \
              EXECUTE IMMEDIATE ('DROP TYPE str_kvp'); \
          EXCEPTION \
              WHEN OTHERS \
              THEN NULL; \
          END; \
          BEGIN \
              EXECUTE IMMEDIATE ('DROP TYPE primitives_udt'); \
          EXCEPTION \
              WHEN OTHERS \
              THEN NULL; \
          END; \
          EXECUTE IMMEDIATE (' \
              CREATE TYPE primitives_udt AS OBJECT ( \
                num     NUMBER, \
                dateVal DATE, \
                rawVal RAW(100)\
              ) \
          '); \
          EXECUTE IMMEDIATE (' \
              CREATE TYPE str_kvp AS OBJECT ( \
                key   varchar2 (30 char) , \
                value varchar2 (2000 char) \
              ) \
          '); \
          EXECUTE IMMEDIATE (' \
              CREATE TYPE str_kvp_nested AS OBJECT ( \
                key   varchar2 (30 char) , \
                value str_kvp \
              ) \
          '); \
          EXECUTE IMMEDIATE (' \
              CREATE TYPE str_kvp_table AS TABLE OF str_kvp \
          '); \
          EXECUTE IMMEDIATE (' \
              CREATE TABLE udt ( \
                  id NUMBER, \
                  udt str_kvp, \
                  primitives primitives_udt \
              ) \
          '); \
          EXECUTE IMMEDIATE (' \
              CREATE TABLE udt_nested ( \
                  id NUMBER, \
                  udt str_kvp_nested \
              ) \
          '); \
          EXECUTE IMMEDIATE (' \
              CREATE TABLE udt_table ( \
                  id NUMBER, \
                  udt str_kvp_table \
              ) \
              NESTED TABLE udt STORE AS udt_table_udts \
          '); \
      END; ";

  const rowsAmount = 218;
  var insertRows =
      "BEGIN \
          FOR i IN 0.." + (rowsAmount - 1) + " LOOP \
             INSERT INTO udt VALUES (i, str_kvp('key ' || i, 'val ' || i), \
                                     primitives_udt(i, to_date('2016-1-1', 'yyyy-mm-dd') + i, to_char(i, 'fm0x'))); \
             INSERT INTO udt_nested VALUES (i, str_kvp_nested('key nested ' || i, str_kvp('key ' || i, 'val ' || i))); \
             INSERT INTO udt_table VALUES (i, str_kvp_table(str_kvp('key ' || i, 'val ' || i), str_kvp(i || 'key ', i || 'val '))); \
          END LOOP; \
       END; ";

  before(function (done) {
    oracledb.getConnection(dbConfig, function(err, conn) {
      if(err) { console.error(err.message); return; }
      connection = conn;
      connection.execute(createTables, function(err) {
        if(err) { console.error(err.message); return; }
        connection.execute(insertRows, function(err) {
          if(err) { console.error(err.message); return; }
          done();
        });
      });
    });
  })

  after(function(done) {
    connection.execute(
      "BEGIN \
          EXECUTE IMMEDIATE ('DROP TABLE udt_table'); \
          EXECUTE IMMEDIATE ('DROP TABLE udt_nested'); \
          EXECUTE IMMEDIATE ('DROP TABLE udt'); \
          EXECUTE IMMEDIATE ('DROP TYPE str_kvp_table'); \
          EXECUTE IMMEDIATE ('DROP TYPE str_kvp_nested'); \
          EXECUTE IMMEDIATE ('DROP TYPE str_kvp'); \
          EXECUTE IMMEDIATE ('DROP TYPE primitives_udt'); \
       END; ",
      function(err) {
        if(err) { console.error(err.message); return; }
        connection.release(function(err) {
          if(err) { console.error(err.message); return; }
          done();
        });
      }
    );
  })

  Date.prototype.addDays = function (days) {
    var dat = new Date(this.valueOf());
    dat.setDate(dat.getDate() + days);
    return dat;
  }
  var testDate = new Date('2016-1-1');

  Number.prototype.toValidHex = function () {
    var hex = this.toString(16);

    return (hex.length % 2 === 0) ? hex : '0' + hex;
  }

  describe('67.1 Testing fetch of UDT object', function() {
    it('67.1.1 when outFormat = oracledb.ARRAY, fetched rows are correct', function (done) {
      connection.should.be.ok();

      connection.execute(
        "SELECT * FROM udt",
        [],
        { outFormat: oracledb.ARRAY, prefetchRows: 100, maxRows: 1000 },
        function(err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i) {
            var rawBuf = new Buffer(i.toValidHex(), 'hex');
            should.deepEqual(result.rows[i], [i, ['key ' + i, 'val ' + i], [i, testDate.addDays(i), rawBuf]]);
          }
          done();
        }
      );
    })

    it('67.1.2 when outFormat = oracledb.OBJECT, fetched rows are correct', function (done) {
      connection.should.be.ok();

      connection.execute(
        "SELECT * FROM udt",
        [],
        { outFormat: oracledb.OBJECT, prefetchRows: 100, maxRows: 1000 },
        function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i) {
            var rawBuf = new Buffer(i.toValidHex(), 'hex');
            should.deepEqual(result.rows[i], {
              ID: i,
              UDT: { KEY: 'key ' + i, VALUE: 'val ' + i },
              PRIMITIVES: { NUM: i, DATEVAL: testDate.addDays(i), RAWVAL: rawBuf }
            });
          }
          done();
        }
      );
    })

  })

  describe('67.2 Testing fetch of UDT object contains nested UDT object', function () {
    it('67.2.1 when outFormat = oracledb.ARRAY, fetched rows are correct', function (done) {
      connection.should.be.ok();

      connection.execute(
        "SELECT * FROM udt_nested",
        [],
        { outFormat: oracledb.ARRAY, prefetchRows: 100, maxRows: 1000 },
        function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], [i, ['key nested ' + i, ['key ' + i, 'val ' + i]]]);
          done();
        }
      );
    })

    it('67.2.2 when outFormat = oracledb.OBJECT, fetched rows are correct', function (done) {
      connection.should.be.ok();

      connection.execute(
        "SELECT * FROM udt_nested",
        [],
        { outFormat: oracledb.OBJECT, prefetchRows: 100, maxRows: 1000 },
        function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], { ID: i, UDT: { KEY: 'key nested ' + i, VALUE: { KEY: 'key ' + i, VALUE: 'val ' + i } } });
          done();
        }
      );
    })
  })

  describe('67.3 Testing fetch of UDT object contains nested table object', function () {
    it('67.3.1 when outFormat = oracledb.ARRAY, fetched rows are correct', function (done) {
      connection.should.be.ok();

      connection.execute(
        "SELECT * FROM udt_table",
        [],
        { outFormat: oracledb.ARRAY, prefetchRows: 100, maxRows: 1000 },
        function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], [i, [['key ' + i, 'val ' + i], [i + 'key ', i + 'val ']]]);
          done();
        }
      );
    })

    it('67.3.2 when outFormat = oracledb.OBJECT, fetched rows are correct', function (done) {
      connection.should.be.ok();

      connection.execute(
        "SELECT * FROM udt_table",
        [],
        { outFormat: oracledb.OBJECT, prefetchRows: 100, maxRows: 1000 },
        function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], { ID: i, UDT: [{ KEY: 'key ' + i, VALUE: 'val ' + i }, { KEY: i + 'key ', VALUE: i + 'val ' }] });
          done();
        }
      );
    })
  })

})

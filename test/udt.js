var oracledb = require('oracledb');
var should   = require('should');
var dbConfig = require('./dbconfig.js');

describe('67 udt.js', function() {
  var connection = null;
  const createQuery = `BEGIN
  DECLARE
    e_table_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_table_missing, -00942);
  BEGIN
    EXECUTE IMMEDIATE('DROP TABLE test_udt');
  EXCEPTION
    WHEN e_table_missing THEN NULL;
  END;
  DECLARE
    e_type_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_type_missing, -04043);
  BEGIN
    EXECUTE IMMEDIATE('DROP TYPE test_udt_num_table');
  EXCEPTION
    WHEN e_type_missing THEN NULL;
  END;
  DECLARE
    e_type_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_type_missing, -04043);
  BEGIN
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_table');
  EXCEPTION
    WHEN e_type_missing THEN NULL;
  END;
  DECLARE
    e_type_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_type_missing, -04043);
  BEGIN
    EXECUTE IMMEDIATE('DROP TYPE test_udt_nested_str_kvp_table');
  EXCEPTION
    WHEN e_type_missing THEN NULL;
  END;
  DECLARE
    e_type_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_type_missing, -04043);
  BEGIN
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_kvp_table');
  EXCEPTION
    WHEN e_type_missing THEN NULL;
  END;
  DECLARE
    e_type_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_type_missing, -04043);
  BEGIN
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_kvp_nested');
  EXCEPTION
    WHEN e_type_missing THEN NULL;
  END;
  DECLARE
    e_type_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_type_missing, -04043);
  BEGIN
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_kvp');
  EXCEPTION
    WHEN e_type_missing THEN NULL;
  END;
  DECLARE
    e_type_missing EXCEPTION;
    PRAGMA EXCEPTION_INIT(e_type_missing, -04043);
  BEGIN
    EXECUTE IMMEDIATE('DROP TYPE test_udt_primitives');
  EXCEPTION
    WHEN e_type_missing THEN NULL;
  END;
  EXECUTE IMMEDIATE('
    CREATE TYPE test_udt_primitives AS OBJECT(
      num     NUMBER,
      dateVal DATE,
      rawVal RAW(100)
    )
  ');
  EXECUTE IMMEDIATE('
    CREATE TYPE test_udt_str_kvp AS OBJECT(
      key   varchar2(30 char) ,
      value varchar2(2000 char)
    )
  ');
  EXECUTE IMMEDIATE('
    CREATE TYPE test_udt_str_kvp_nested AS OBJECT(
      key   varchar2(30 char) ,
      value test_udt_str_kvp
    )
  ');
  EXECUTE IMMEDIATE('
    CREATE TYPE test_udt_num_table AS TABLE OF number
  ');
  EXECUTE IMMEDIATE('
    CREATE TYPE test_udt_str_table AS TABLE OF VARCHAR2(200)
  ');
  EXECUTE IMMEDIATE('
    CREATE TYPE test_udt_str_kvp_table AS TABLE OF test_udt_str_kvp
  ');
  EXECUTE IMMEDIATE('
    CREATE TYPE test_udt_nested_str_kvp_table AS TABLE OF test_udt_str_kvp_nested
  ');
  EXECUTE IMMEDIATE('
    CREATE TABLE test_udt(
        id NUMBER,
        primitives test_udt_primitives,
        kvp test_udt_str_kvp,
        kvp_nested test_udt_str_kvp_nested,
        num_tab test_udt_num_table,
        str_tab test_udt_str_table,
        tab test_udt_str_kvp_table,
        nested_tab test_udt_nested_str_kvp_table
    )
    NESTED TABLE num_tab STORE AS test_udt_num_tab,
    NESTED TABLE str_tab STORE AS test_udt_str_tab,
    NESTED TABLE tab STORE AS test_udt_tab,
    NESTED TABLE nested_tab STORE AS test_udt_nested_tab
  ');
  END;`;
  const dropQuery = `BEGIN
    EXECUTE IMMEDIATE('DROP TABLE test_udt');
    EXECUTE IMMEDIATE('DROP TYPE test_udt_num_table');
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_table');
    EXECUTE IMMEDIATE('DROP TYPE test_udt_nested_str_kvp_table');
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_kvp_table');
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_kvp_nested');
    EXECUTE IMMEDIATE('DROP TYPE test_udt_str_kvp');
    EXECUTE IMMEDIATE('DROP TYPE test_udt_primitives');
  END;`;

  before(function (done) {
    oracledb.getConnection(dbConfig, function(err, conn) {
      should.not.exist(err);
      connection = conn;
      connection.execute(createQuery, function(err) {
        should.not.exist(err);
        done();
      });
    });
  })

  after(function(done) {
    connection.execute(dropQuery, function(err) {
      should.not.exist(err);
      connection.release(function(err) {
        should.not.exist(err);
        done();
      });
    });
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

  describe('67.1 fetch', function() {
    const rowsAmount = 218;
    const insertQuery = `BEGIN
      FOR i IN 0..${rowsAmount - 1} LOOP
        INSERT INTO test_udt(id, kvp, primitives, kvp_nested, tab) VALUES(
          i,
          test_udt_str_kvp('key ' || i, 'val ' || i),
          test_udt_primitives(i, to_date('2016-1-1', 'yyyy-mm-dd') + i, to_char(i, 'fm0x')),
          test_udt_str_kvp_nested('key nested ' || i, test_udt_str_kvp('key ' || i, 'val ' || i)),
          test_udt_str_kvp_table(test_udt_str_kvp('key ' || i, 'val ' || i), test_udt_str_kvp(i || 'key ', i || 'val '))
        );
      END LOOP;
    END;`;
    const deleteQuery = `BEGIN
      EXECUTE IMMEDIATE('delete from test_udt');
    END;`;

    before(function(done) {
      connection.execute(insertQuery, function(err) {
        should.not.exist(err);
        done();
      });
    })

    after(function(done) {
      connection.execute(deleteQuery, function(err) {
        should.not.exist(err);
        done();
      });
    })

    describe('67.1.1 primitives', function() {
      var query = "SELECT id, kvp, primitives FROM test_udt order by id";

      it('67.1.1.1 when outFormat = oracledb.ARRAY', function (done) {
        connection.should.be.ok();

        connection.execute(query, [], { outFormat: oracledb.ARRAY, maxRows: rowsAmount }, function(err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i) {
            var rawBuf = new Buffer(i.toValidHex(), 'hex');
            should.deepEqual(result.rows[i], [i, ['key ' + i, 'val ' + i], [i, testDate.addDays(i), rawBuf]]);
          }
          done();
        });
      })

      it('67.1.1.2 when outFormat = oracledb.OBJECT', function (done) {
        connection.should.be.ok();

        connection.execute(query, [], { outFormat: oracledb.OBJECT, maxRows: rowsAmount }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i) {
            var rawBuf = new Buffer(i.toValidHex(), 'hex');
            should.deepEqual(result.rows[i], {
              ID: i,
              KVP: { KEY: 'key ' + i, VALUE: 'val ' + i },
              PRIMITIVES: { NUM: i, DATEVAL: testDate.addDays(i), RAWVAL: rawBuf }
            });
          }
          done();
        });
      })

      it('67.1.1.3 contains null values', function (done) {
        connection.should.be.ok();
        const queryNulls = "SELECT test_udt_primitives(NULL, NULL, NULL) as obj FROM dual";

        connection.execute(queryNulls, [], { outFormat: oracledb.OBJECT }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(1);

          should.deepEqual(result.rows[0], { OBJ: { NUM: null, DATEVAL: null, RAWVAL: null } });

          done();
        });
      })

    })

    describe('67.1.2 nested UDT object', function () {
      var query = "SELECT id, kvp_nested FROM test_udt order by id";

      it('67.1.2.1 when outFormat = oracledb.ARRAY', function (done) {
        connection.should.be.ok();

        connection.execute(query, [], { outFormat: oracledb.ARRAY, maxRows: rowsAmount }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], [i, ['key nested ' + i, ['key ' + i, 'val ' + i]]]);
          done();
        });
      })

      it('67.1.2.2 when outFormat = oracledb.OBJECT', function (done) {
        connection.should.be.ok();

        connection.execute(query, [], { outFormat: oracledb.OBJECT, maxRows: rowsAmount }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], { ID: i, KVP_NESTED: { KEY: 'key nested ' + i, VALUE: { KEY: 'key ' + i, VALUE: 'val ' + i } } });
          done();
        });
      })

      it('67.1.2.3 contains null values', function (done) {
        connection.should.be.ok();
        const queryNulls = "SELECT test_udt_str_kvp_nested(NULL, test_udt_str_kvp(NULL, NULL)) as NESTED_OBJ FROM dual";

        connection.execute(queryNulls, [], { outFormat: oracledb.OBJECT }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(1);

          should.deepEqual(result.rows[0], { NESTED_OBJ: { KEY: null, VALUE: { KEY: null, VALUE: null } } });

          done();
        });
      })
    })

    describe('67.1.3 nested table', function () {
      var query = "SELECT id, tab FROM test_udt order by id";

      it('67.1.3.1 when outFormat = oracledb.ARRAY', function (done) {
        connection.should.be.ok();

        connection.execute(query, [], { outFormat: oracledb.ARRAY, maxRows: rowsAmount }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], [i, [['key ' + i, 'val ' + i], [i + 'key ', i + 'val ']]]);
          done();
        });
      })

      it('67.1.3.2 when outFormat = oracledb.OBJECT', function (done) {
        connection.should.be.ok();

        connection.execute(query, [], { outFormat: oracledb.OBJECT, maxRows: rowsAmount }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(rowsAmount);
          for (var i = 0; i < rowsAmount; ++i)
            should.deepEqual(result.rows[i], { ID: i, TAB: [{ KEY: 'key ' + i, VALUE: 'val ' + i }, { KEY: i + 'key ', VALUE: i + 'val ' }] });
          done();
        });
      })

      it('67.1.3.3 contains null values', function (done) {
        connection.should.be.ok();
        const queryNulls = "SELECT test_udt_num_table(NULL, NULL) as primitives_tab, test_udt_str_kvp_table(NULL, NULL, NULL) as tab FROM dual";

        connection.execute(queryNulls, [], { outFormat: oracledb.OBJECT }, function (err, result) {
          should.not.exist(err);

          should.exist(result.rows);
          result.rows.length.should.be.exactly(1);

          should.deepEqual(result.rows[0], { PRIMITIVES_TAB: [ null, null ], TAB: [ null, null, null ] });

          done();
        });
      })
    })
  })

  describe('67.2 IN bind', function() {
    beforeEach(function (done) {
      connection.execute("delete from test_udt", function(err) {
        should.not.exist(err);
        done();
      });
    })

    it('67.2.1 primitives', function(done) {
      connection.should.be.ok();

      const PRIMITIVES_OBJ = { NUM: 777, DATEVAL: testDate, RAWVAL: new Buffer('0A', 'hex') };
      connection.execute("insert into test_udt(primitives) values(:p)",
        {
          p: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: PRIMITIVES_OBJ,
            udtName: 'TEST_UDT_PRIMITIVES'
          }
        },
        function (err, result) {
          should.not.exist(err);

          result.rowsAffected.should.be.exactly(1);
          connection.execute("SELECT primitives FROM test_udt", [], { outFormat: oracledb.OBJECT }, function (err, result) {
            should.not.exist(err);

            should.exist(result.rows);
            result.rows.length.should.be.exactly(1);
            should.deepEqual(result.rows[0], { PRIMITIVES: PRIMITIVES_OBJ });
            done();
          });
        }
      );
    });

    it('67.2.2 nested object', function(done) {
      connection.should.be.ok();

      const OBJ = { KEY: 'root key', VALUE: { KEY: 'nested key', VALUE: 'nested value' } };
      connection.execute("insert into test_udt(kvp_nested) values(:obj)",
        {
          obj: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: OBJ,
            udtName: 'TEST_UDT_STR_KVP_NESTED'
          }
        },
        function (err, result) {
          should.not.exist(err);

          result.rowsAffected.should.be.exactly(1);
          connection.execute("SELECT kvp_nested FROM test_udt", [], { outFormat: oracledb.OBJECT }, function (err, result) {
            should.not.exist(err);

            should.exist(result.rows);
            result.rows.length.should.be.exactly(1);
            should.deepEqual(result.rows[0], { KVP_NESTED: OBJ });
            done();
          });
        }
      );
    });

    describe('67.2.3 nested table', function() {
      it('67.2.3.1 array of primitives', function(done) {
        connection.should.be.ok();

        const NUM_ARR = [45, 30, 67], STR_ARR = ["first", "second"];
        connection.execute("insert into test_udt(num_tab, str_tab) values(:nums, :strs)",
          {
            nums: {
              type: oracledb.UDT,
              dir: oracledb.BIND_IN,
              val: NUM_ARR,
              udtName: 'TEST_UDT_NUM_TABLE'
            },
            strs: {
              type: oracledb.UDT,
              dir: oracledb.BIND_IN,
              val: STR_ARR,
              udtName: 'TEST_UDT_STR_TABLE'
            }
          },
          function (err, result) {
            should.not.exist(err);

            result.rowsAffected.should.be.exactly(1);
            connection.execute("SELECT num_tab, str_tab FROM test_udt", [], { outFormat: oracledb.OBJECT }, function (err, result) {
              should.not.exist(err);

              should.exist(result.rows);
              result.rows.length.should.be.exactly(1);
              should.deepEqual(result.rows[0], { NUM_TAB: NUM_ARR, STR_TAB: STR_ARR });
              done();
            });
          }
        );
      });

      it('67.2.3.2 array of objects', function (done) {
        connection.should.be.ok();

        const OBJ_ARR = [{ KEY: "key1", VALUE: "val1" }, { KEY: "key2", VALUE: "val2" }];
        connection.execute("insert into test_udt(tab) values(:tab)",
          {
            tab: {
              type: oracledb.UDT,
              dir: oracledb.BIND_IN,
              val: OBJ_ARR,
              udtName: 'TEST_UDT_STR_KVP_TABLE'
            }
          },
          function (err, result) {
            should.not.exist(err);

            result.rowsAffected.should.be.exactly(1);
            connection.execute("SELECT tab FROM test_udt", [], { outFormat: oracledb.OBJECT },
              function (err, result) {
                should.not.exist(err);

                should.exist(result.rows);
                result.rows.length.should.be.exactly(1);
                should.deepEqual(result.rows[0], { TAB: OBJ_ARR });
                done();
              }
            );
          }
        );
      });

      it('67.2.3.3 array of nested objects', function (done) {
        connection.should.be.ok();

        const OBJ_ARR = [{ KEY: "key1", VALUE: { KEY: "nested key 1", VALUE: "val1" } },
                         { KEY: "key2", VALUE: { KEY: "nested key 2", VALUE: "val2" } }];
        connection.execute("insert into test_udt(nested_tab) values(:objs)",
          {
            objs: {
              type: oracledb.UDT,
              dir: oracledb.BIND_IN,
              val: OBJ_ARR,
              udtName: 'TEST_UDT_NESTED_STR_KVP_TABLE'
            }
          },
          function (err, result) {
            should.not.exist(err);

            result.rowsAffected.should.be.exactly(1);
            connection.execute("SELECT nested_tab FROM test_udt", [], { outFormat: oracledb.OBJECT },
              function (err, result) {
                should.not.exist(err);

                should.exist(result.rows);
                result.rows.length.should.be.exactly(1);
                should.deepEqual(result.rows[0], { NESTED_TAB: OBJ_ARR });
                done();
              }
            );
          }
        );
      });
    });

    it('67.2.4 null', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(kvp_nested) values(:obj)",
        {
          obj: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: null,
            udtName: 'TEST_UDT_STR_KVP_NESTED'
          }
        },
        function (err, result) {
          should.not.exist(err);

          result.rowsAffected.should.be.exactly(1);
          connection.execute("SELECT kvp_nested FROM test_udt", [], { outFormat: oracledb.OBJECT }, function (err, result) {
            should.not.exist(err);

            should.exist(result.rows);
            result.rows.length.should.be.exactly(1);
            should.deepEqual(result.rows[0], { KVP_NESTED: null });
            done();
          });
        }
      );
    });

    it('67.2.5 udtName is not provided - error expected', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(id) values(:id)",
        {
          id: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: 1
          }
        },
        function (err, result) {
          err.message.should.containEql('udtName option required for UDT binds');
          done();
        }
      );
    });

    it('67.2.6 invalid udtName provided - error expected', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(num_tab) values(:nums)",
        {
          nums: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: [1, 2],
            udtName: 'INVALID'
          }
        },
        function (err, result) {
          err.message.should.containEql('"INVALID" not found');
          done();
        }
      );
    });

    it('67.2.7 val doesnt match udtName - error expected', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(num_tab) values(:nums)",
        {
          nums: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: ['test', 'some string'],
            udtName: 'test_udt_num_table'
          }
        },
        function (err, result) {
          err.message.should.containEql('number value required for UDT bind');
          done();
        }
      );
    });

    it('67.2.8 nested val doesnt match oracle - extra fields ignored, missing becomes null', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(tab) values(:tab)",
        {
          tab: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: [{ KEY: "k", EXTRA_FIELD: "extra" }],
            udtName: 'TEST_UDT_STR_KVP_TABLE'
          }
        },
        function (err, result) {
          should.not.exist(err);

          result.rowsAffected.should.be.exactly(1);
          connection.execute("SELECT tab FROM test_udt", [], { outFormat: oracledb.OBJECT }, function (err, result) {
            should.not.exist(err);

            should.exist(result.rows);
            result.rows.length.should.be.exactly(1);
            should.deepEqual(result.rows[0], { TAB: [ { KEY: 'k', VALUE: null } ] });
            done();
          });
        }
      );
    });

    it('67.2.9 val fields are case insensitive', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(kvp) values(:kvp)",
        {
          kvp: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: { kEy: "k", ValuE: "v" },
            udtName: 'TEST_UDT_STR_KVP'
          }
        },
        function (err, result) {
          should.not.exist(err);

          result.rowsAffected.should.be.exactly(1);
          connection.execute("SELECT kvp FROM test_udt", [], { outFormat: oracledb.OBJECT }, function (err, result) {
            should.not.exist(err);

            should.exist(result.rows);
            result.rows.length.should.be.exactly(1);
            should.deepEqual(result.rows[0], { KVP: { KEY: 'k', VALUE: 'v' } });
            done();
          });
        }
      );
    });

    it('67.2.10 val datatype is invalid - error expected', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(num_tab) values(:nums)",
        {
          nums: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: 666,
            udtName: 'TEST_UDT_NUM_TABLE'
          }
        },
        function (err, result) {
          err.message.should.containEql('only js array or object allowed for UDT binds');
          done();
        }
      );
    });

    it('67.2.11 val datatype doesnt match oracle - error expected', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(num_tab) values(:nums)",
        {
          nums: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: {a: 1},
            udtName: 'TEST_UDT_NUM_TABLE'
          }
        },
        function (err, result) {
          err.message.should.containEql('js object binding possible only to oracle user defined datatype');
          done();
        }
      );
    });

    it('67.2.12 duplicated case-insensitive fields in js obj - error expected', function(done) {
      connection.should.be.ok();

      connection.execute("insert into test_udt(kvp) values(:kvp)",
        {
          kvp: {
            type: oracledb.UDT,
            dir: oracledb.BIND_IN,
            val: {key: "a", KeY: "b"},
            udtName: 'TEST_UDT_STR_KVP'
          }
        },
        function (err, result) {
          err.message.should.containEql('Js object contains duplicated case-insensitive fields key and KeY');
          done();
        }
      );
    });

  });

})

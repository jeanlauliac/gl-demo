/* @flow */

'use strict'

import immutable from 'immutable'

export default class LightweightImmutable {

  _immutableData: {fieldMap: ?immutable._Map<string, mixed>};

  constructor(fields: Object) {
    Object.defineProperty(this, '_immutableData', {value: {fieldMap: null}})
    Object.assign(this, fields)
    Object.freeze(this)
  }

  valueOf(): immutable._Map<string, mixed> {
    let fieldMap = this._immutableData.fieldMap
    if (fieldMap == null) {
      fieldMap = this._immutableData.fieldMap = immutable.Map(this)
    }
    return fieldMap
  }

}

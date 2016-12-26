'use strict';

class Updfile {

  constructor() {
    this._nextID = 1;
    this.data = {
      groups: {},

    };
  }

  _newGroup(obj) {
    const id = this._nextID++;
    this.data.groups[id] = obj;
    return id;
  }

  glob(pattern) {
    return this._newGroup({
      type: 'glob',
      pattern,
    });
  }

  rule(command, inputGroup, outputPattern) {
    return this._newGroup({
      type: 'rule',
      command,
      inputGroup,
      outputPattern,
    })
  }

  alias(name, inputGroup) {
    return this._newGroup({
      type: 'alias',
      name,
      inputGroup,
    });
  }

}

function export_(fun) {
  const updfile = new Updfile();
  fun(updfile);
  console.log(JSON.stringify(updfile.data));
}

module.exports = {
  export: export_,
};
